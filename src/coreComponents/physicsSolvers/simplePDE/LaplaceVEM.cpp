/*
 * ------------------------------------------------------------------------------------------------------------
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 * Copyright (c) 2018-2019 Lawrence Livermore National Security LLC
 * Copyright (c) 2018-2019 The Board of Trustees of the Leland Stanford Junior University
 * Copyright (c) 2018-2019 Total, S.A
 * Copyright (c) 2019-     GEOSX Contributors
 * All right reserved
 *
 * See top level LICENSE, COPYRIGHT, CONTRIBUTORS, NOTICE, and ACKNOWLEDGEMENTS files for details.
 * ------------------------------------------------------------------------------------------------------------
 */

/**
 * @file LaplaceVEM.cpp
 *
 */

#include "LaplaceVEM.hpp"

#include <vector>
#include <math.h>

#include "mpiCommunications/CommunicationTools.hpp"
#include "mpiCommunications/NeighborCommunicator.hpp"
#include "dataRepository/Group.hpp"
#include "common/TimingMacros.hpp"

#include "common/DataTypes.hpp"
#include "constitutive/ConstitutiveManager.hpp"
#include "finiteElement/FiniteElementDiscretizationManager.hpp"
#include "finiteElement/ElementLibrary/FiniteElement.h"
#include "finiteElement/Kinematics.h"
#include "managers/NumericalMethodsManager.hpp"
#include "codingUtilities/Utilities.hpp"

#include "managers/DomainPartition.hpp"

namespace geosx
{

namespace dataRepository
{
namespace keys
{}
}

using namespace dataRepository;
using namespace constitutive;


	//START_SPHINX_INCLUDE_01
LaplaceVEM::LaplaceVEM( const std::string& name,
												Group * const parent ):
	SolverBase( name, parent ),
	m_fieldName("primaryField")
{
	registerWrapper<string>(LaplaceVEMViewKeys.timeIntegrationOption.Key())->
		setInputFlag(InputFlags::REQUIRED)->
		setDescription("option for default time integration method");

	registerWrapper<string>(LaplaceVEMViewKeys.fieldVarName.Key(), &m_fieldName, false)->
		setInputFlag(InputFlags::REQUIRED)->
		setDescription("name of field variable");
}
	//END_SPHINX_INCLUDE_01

LaplaceVEM::~LaplaceVEM()
{
	// TODO Auto-generated destructor stub
}


	//START_SPHINX_INCLUDE_02
void LaplaceVEM::RegisterDataOnMesh( Group * const MeshBodies )
{
	for( auto & mesh : MeshBodies->GetSubGroups() )
	{
		NodeManager * const nodes = mesh.second->group_cast<MeshBody*>()->getMeshLevel(0)->getNodeManager();

		nodes->registerWrapper<real64_array >( m_fieldName )->
			setApplyDefaultValue(0.0)->
			setPlotLevel(PlotLevel::LEVEL_0)->
			setDescription("Primary field variable");
	}
}
	//END_SPHINX_INCLUDE_02

	//START_SPHINX_INCLUDE_03
void LaplaceVEM::PostProcessInput()
{
	SolverBase::PostProcessInput();

	string tiOption = this->getReference<string>(LaplaceVEMViewKeys.timeIntegrationOption);

	if( tiOption == "SteadyState" )
	{
		this->m_timeIntegrationOption = timeIntegrationOption::SteadyState;
	}
	else if( tiOption == "ImplicitTransient" )
	{
		this->m_timeIntegrationOption = timeIntegrationOption::ImplicitTransient;
	}
	else if ( tiOption == "ExplicitTransient" )
	{
		this->m_timeIntegrationOption = timeIntegrationOption::ExplicitTransient;
	}
	else
	{
		GEOS_ERROR("invalid time integration option");
	}

	// Set basic parameters for solver
	// m_linearSolverParameters.verbosity = 0;
	// m_linearSolverParameters.solverType = "gmres";
	// m_linearSolverParameters.krylov.tolerance = 1e-8;
	// m_linearSolverParameters.krylov.maxIterations = 250;
	// m_linearSolverParameters.krylov.maxRestart = 250;
	// m_linearSolverParameters.preconditionerType = "amg";
	// m_linearSolverParameters.amg.smootherType = "gaussSeidel";
	// m_linearSolverParameters.amg.coarseType = "direct";
}
	//END_SPHINX_INCLUDE_03

real64 LaplaceVEM::SolverStep( real64 const& time_n,
															 real64 const& dt,
															 const int cycleNumber,
															 DomainPartition * domain )
{
	real64 dtReturn = dt;
	if( m_timeIntegrationOption == timeIntegrationOption::ExplicitTransient )
	{
		dtReturn = ExplicitStep( time_n, dt, cycleNumber, domain );
	}
	else if( m_timeIntegrationOption == timeIntegrationOption::ImplicitTransient ||
					 m_timeIntegrationOption == timeIntegrationOption::SteadyState )
	{
		dtReturn = this->LinearImplicitStep( time_n, dt, cycleNumber, domain, m_dofManager, m_matrix, m_rhs, m_solution );
	}
	return dtReturn;
}

real64 LaplaceVEM::ExplicitStep( real64 const& GEOSX_UNUSED_ARG( time_n ),
																 real64 const& dt,
																 const int GEOSX_UNUSED_ARG( cycleNumber ),
																 DomainPartition * const GEOSX_UNUSED_ARG( domain ) )
{
	return dt;
}

void LaplaceVEM::ImplicitStepSetup( real64 const & GEOSX_UNUSED_ARG( time_n ),
																		real64 const & GEOSX_UNUSED_ARG( dt ),
																		DomainPartition * const domain,
																		DofManager & dofManager,
																		ParallelMatrix & matrix,
																		ParallelVector & rhs,
																		ParallelVector & solution )
{
	// Computation of the sparsity pattern
	SetupSystem( domain, dofManager, matrix, rhs, solution );
}

void LaplaceVEM::ImplicitStepComplete( real64 const & GEOSX_UNUSED_ARG( time_n ),
																			 real64 const & GEOSX_UNUSED_ARG( dt ),
																			 DomainPartition * const GEOSX_UNUSED_ARG( domain ) )
{
}

void LaplaceVEM::SetupDofs( DomainPartition const * const GEOSX_UNUSED_ARG( domain ),
														DofManager & dofManager ) const
{
	dofManager.addField( m_fieldName,
											 DofManager::Location::Node,
											 DofManager::Connectivity::Elem );
}

//START_SPHINX_INCLUDE_04
void LaplaceVEM::AssembleSystem( real64 const time_n,
																 real64 const GEOSX_UNUSED_ARG( dt ),
																 DomainPartition * const domain,
																 DofManager const & dofManager,
																 ParallelMatrix & matrix,
																 ParallelVector & rhs )
{
	MeshLevel * const mesh = domain->getMeshBodies()->GetGroup<MeshBody>(0)->getMeshLevel(0);
	NodeManager const * const nodeManager = mesh->getNodeManager();
	// EdgeManager const * const edgeManager = mesh->getEdgeManager();
	FaceManager const * const faceManager = mesh->getFaceManager();
	ElementRegionManager * const elemManager = mesh->getElemManager();
	NumericalMethodsManager const *
	numericalMethodManager = domain->getParent()->GetGroup<NumericalMethodsManager>(keys::numericalMethodsManager);
	FiniteElementDiscretizationManager const *
	feDiscretizationManager = numericalMethodManager->
		GetGroup<FiniteElementDiscretizationManager>(keys::finiteElementDiscretizations);

	array1d<globalIndex> const &
	dofIndex = nodeManager->getReference< array1d<globalIndex> >( dofManager.getKey( m_fieldName ) );

	// arrayView1d<R1Tensor const> const & X = nodeManager->referencePosition();



	// arrayView1d<real64 const> const & faceArea = faceManager->faceArea();
	// arrayView1d<R1Tensor const> const & faceCenter = faceManager->faceCenter();
	// arrayView1d<R1Tensor const> const & faceNormal = faceManager->faceNormal();

	// arrayView1d<R1Tensor const> const & edgeCenter = edgeManager->edgeCenter();



	// Initialize all entries to zero
	matrix.zero();
	rhs.zero();

	matrix.open();
	rhs.open();



	// begin region loop
	for( localIndex er=0 ; er<elemManager->numRegions() ; ++er )
	{
		ElementRegionBase * const elementRegion = elemManager->GetRegion(er);

		FiniteElementDiscretization const *
		feDiscretization = feDiscretizationManager->GetGroup<FiniteElementDiscretization>(m_discretizationName);

		elementRegion->forElementSubRegionsIndex<CellElementSubRegion>([&]( localIndex const GEOSX_UNUSED_ARG( esr ),
																																				CellElementSubRegion const * const elementSubRegion )
		{
			array3d<R1Tensor> const &
			dNdX = elementSubRegion->getReference< array3d< R1Tensor > >(keys::dNdX);

			arrayView2d<real64> const &
			detJ = elementSubRegion->getReference< array2d<real64> >(keys::detJ);

			localIndex const numNodesPerElement = elementSubRegion->numNodesPerElement();
			arrayView2d<localIndex const, CellBlock::NODE_MAP_UNIT_STRIDE_DIM> const & elemNodes = elementSubRegion->nodeList();

			globalIndex_array elemDofIndex( numNodesPerElement );
			real64_array element_rhs( numNodesPerElement );
			real64_array2d element_matrix( numNodesPerElement, numNodesPerElement );

			integer_array const & elemGhostRank = elementSubRegion->m_ghostRank;
			localIndex const n_q_points = feDiscretization->m_finiteElement->n_quadrature_points();

			arrayView2d<localIndex> elemsToFaces = elementSubRegion->faceList();

			localIndex const numFacesPerElem = elemsToFaces.size(1);

			ArrayOfArraysView< localIndex const > const & facesToNodes = faceManager->nodeList();

			// begin element loop, skipping ghost elements
			for( localIndex k=0 ; k<elementSubRegion->size() ; ++k )
			{
				for( localIndex kf=0 ; kf<numFacesPerElem ; kf++ )
				{
					localIndex const faceIndex = elemsToFaces(k,kf);
					// double area = faceArea(faceIndex);
					// R1Tensor center = faceCenter(faceIndex);
					// R1Tensor normal = faceNormal(faceIndex);

					localIndex const numNodesPerFace = facesToNodes.sizeOfArray(faceIndex);
					for( localIndex a=0 ; a<numNodesPerFace ; ++a )
					{
						// localIndex nodeIndex = facesToNodes(faceIndex,a);

						// R1Tensor nodeCoords = X[nodeIndex];

					}

				}




				if(elemGhostRank[k] < 0)
				{
					element_rhs = 0.0;
					element_matrix = 0.0;
					for( localIndex q=0 ; q<n_q_points ; ++q)
					{
						for( localIndex a=0 ; a<numNodesPerElement ; ++a)
						{
							elemDofIndex[a] = dofIndex[ elemNodes( k, a ) ];

							real64 diffusion = 1.0;
							for( localIndex b=0 ; b<numNodesPerElement ; ++b)
							{
								element_matrix(a,b) += detJ[k][q] *
																			 diffusion *
																		 + Dot( dNdX[k][q][a], dNdX[k][q][b] );
							}

						}
					}
					matrix.add( elemDofIndex, elemDofIndex, element_matrix );
					rhs.add( elemDofIndex, element_rhs );
				}
			}
		});
	}
	matrix.close();
	rhs.close();
	//END_SPHINX_INCLUDE_04

	if( verboseLevel() == 2 )
	{
		GEOS_LOG_RANK_0( "After LaplaceVEM::AssembleSystem" );
		GEOS_LOG_RANK_0("\nJacobian:\n");
		std::cout << matrix;
		GEOS_LOG_RANK_0("\nResidual:\n");
		std::cout << rhs;
	}

	if( verboseLevel() >= 3 )
	{
		SystemSolverParameters * const solverParams = getSystemSolverParameters();
		integer newtonIter = solverParams->numNewtonIterations();

		string filename_mat = "matrix_" + std::to_string( time_n ) + "_" + std::to_string( newtonIter ) + ".mtx";
		matrix.write( filename_mat, true );

		string filename_rhs = "rhs_" + std::to_string( time_n ) + "_" + std::to_string( newtonIter ) + ".mtx";
		rhs.write( filename_rhs, true );

		GEOS_LOG_RANK_0( "After LaplaceVEM::AssembleSystem" );
		GEOS_LOG_RANK_0( "Jacobian: written to " << filename_mat );
		GEOS_LOG_RANK_0( "Residual: written to " << filename_rhs );
	}
}

void LaplaceVEM::ApplySystemSolution( DofManager const & dofManager,
																			ParallelVector const & solution,
																			real64 const scalingFactor,
																			DomainPartition * const domain )
{
	MeshLevel * const mesh = domain->getMeshBody( 0 )->getMeshLevel( 0 );
	NodeManager * const nodeManager = mesh->getNodeManager();

	dofManager.copyVectorToField( solution, m_fieldName, scalingFactor, nodeManager, m_fieldName );

	// Syncronize ghost nodes
	std::map<string, string_array> fieldNames;
	fieldNames["node"].push_back( m_fieldName );

	CommunicationTools::
	SynchronizeFields( fieldNames, mesh,
										 domain->getReference<array1d<NeighborCommunicator> >( domain->viewKeys.neighbors ) );
}

void LaplaceVEM::ApplyBoundaryConditions( real64 const time_n,
																					real64 const dt,
																					DomainPartition * const domain,
																					DofManager const & dofManager,
																					ParallelMatrix & matrix,
																					ParallelVector & rhs )
{
	ApplyDirichletBC_implicit( time_n + dt, dofManager, *domain, m_matrix, m_rhs );

	if( verboseLevel() == 2 )
	{
		GEOS_LOG_RANK_0( "After LaplaceVEM::ApplyBoundaryConditions" );
		GEOS_LOG_RANK_0("\nJacobian:\n");
		std::cout << matrix;
		GEOS_LOG_RANK_0("\nResidual:\n");
		std::cout << rhs;
	}

	if( verboseLevel() >= 3 )
	{
		SystemSolverParameters * const solverParams = getSystemSolverParameters();
		integer newtonIter = solverParams->numNewtonIterations();

		string filename_mat = "matrix_bc_" + std::to_string( time_n ) + "_" + std::to_string( newtonIter ) + ".mtx";
		matrix.write( filename_mat, true );

		string filename_rhs = "rhs_bc_" + std::to_string( time_n ) + "_" + std::to_string( newtonIter ) + ".mtx";
		rhs.write( filename_rhs, true );

		GEOS_LOG_RANK_0( "After LaplaceVEM::ApplyBoundaryConditions" );
		GEOS_LOG_RANK_0( "Jacobian: written to " << filename_mat );
		GEOS_LOG_RANK_0( "Residual: written to " << filename_rhs );
	}
}

void LaplaceVEM::SolveSystem( DofManager const & dofManager,
															ParallelMatrix & matrix,
															ParallelVector & rhs,
															ParallelVector & solution )
{
	rhs.scale( -1.0 ); // TODO decide if we want this here
	solution.zero();

	SolverBase::SolveSystem( dofManager, matrix, rhs, solution );

	if( verboseLevel() == 2 )
	{
		GEOS_LOG_RANK_0("After LaplaceVEM::SolveSystem");
		GEOS_LOG_RANK_0("\nSolution\n");
		std::cout << solution;
	}
}

void LaplaceVEM::ApplyDirichletBC_implicit( real64 const time,
																						DofManager const & dofManager,
																						DomainPartition & domain,
																						ParallelMatrix & matrix,
																						ParallelVector & rhs )
{
	FieldSpecificationManager const * const fsManager = FieldSpecificationManager::get();

	fsManager->Apply( time,
										&domain,
										"nodeManager",
										m_fieldName,
										[&]( FieldSpecificationBase const * const bc,
										string const &,
										set<localIndex> const & targetSet,
										Group * const targetGroup,
										string const GEOSX_UNUSED_ARG( fieldName ) )->void
	{
		bc->ApplyBoundaryConditionToSystem<FieldSpecificationEqual, LAInterface>( targetSet,
																																							false,
																																							time,
																																							targetGroup,
																																							m_fieldName,
																																							dofManager.getKey( m_fieldName ),
																																							1,
																																							matrix,
																																							rhs );
	});
}
//START_SPHINX_INCLUDE_00
REGISTER_CATALOG_ENTRY( SolverBase, LaplaceVEM, std::string const &, Group * const )
//END_SPHINX_INCLUDE_00
} /* namespace ANST */