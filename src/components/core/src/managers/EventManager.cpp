/*
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Copyright (c) 2018, Lawrence Livermore National Security, LLC.
 *
 * Produced at the Lawrence Livermore National Laboratory
 *
 * LLNL-CODE-746361
 *
 * All rights reserved. See COPYRIGHT for details.
 *
 * This file is part of the GEOSX Simulation Framework.
 *
 * GEOSX is a free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License (as published by the
 * Free Software Foundation) version 2.1 dated February 1999.
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

/**
 * @file EventManager.cpp
 */

#include "EventManager.hpp"

#include "managers/DomainPartition.hpp"
#include "managers/Events/EventBase.hpp"

#include "DocumentationNode.hpp"

#include "mesh/MeshLevel.hpp"
#include "mesh/MeshBody.hpp"


namespace geosx
{

using namespace dataRepository;
using namespace cxx_utilities;


EventManager::EventManager( std::string const & name,
                            ManagedGroup * const parent ):
  ManagedGroup( name, parent)
{}


EventManager::~EventManager()
{}


void EventManager::FillDocumentationNode()
{
  cxx_utilities::DocumentationNode * const docNode = this->getDocumentationNode();

  // Set the name to SolverApplications for now
  docNode->setName("Events");
  docNode->setSchemaType("Node");
  docNode->setShortDescription("Contains the set of solver applications");

  docNode->AllocateChildNode( viewKeys.time.Key(),
                              viewKeys.time.Key(),
                              -1,
                              "real64",
                              "real64",
                              "simulation time",
                              "simulation time",
                              "0.0",
                              "",
                              0,
                              1,
                              0 );

  docNode->AllocateChildNode( viewKeys.dt.Key(),
                              viewKeys.dt.Key(),
                              -1,
                              "real64",
                              "real64",
                              "simulation dt",
                              "simulation dt",
                              "0.0",
                              "",
                              0,
                              1,
                              0 );

  docNode->AllocateChildNode( viewKeys.cycle.Key(),
                              viewKeys.cycle.Key(),
                              -1,
                              "integer",
                              "integer",
                              "simulation cycle",
                              "simulation cycle",
                              "0",
                              "",
                              0,
                              1,
                              0 );

  docNode->AllocateChildNode( viewKeys.maxTime.Key(),
                              viewKeys.maxTime.Key(),
                              -1,
                              "real64",
                              "real64",
                              "simulation maxTime",
                              "simulation maxTime",
                              "1e9",
                              "",
                              0,
                              1,
                              0 );

  docNode->AllocateChildNode( viewKeys.maxCycle.Key(),
                              viewKeys.maxCycle.Key(),
                              -1,
                              "integer",
                              "integer",
                              "simulation maxCycle",
                              "simulation maxCycle",
                              "-1",
                              "",
                              0,
                              1,
                              0 );

  docNode->AllocateChildNode( viewKeys.verbosity.Key(),
                              viewKeys.verbosity.Key(),
                              -1,
                              "integer",
                              "integer",
                              "event manager verbosity",
                              "event manager verbosity",
                              "0",
                              "",
                              0,
                              1,
                              0 );
}


void EventManager::CreateChild( string const & childKey, string const & childName )
{
  std::cout << "Adding Event: " << childKey << ", " << childName << std::endl;
  std::unique_ptr<EventBase> event = EventBase::CatalogInterface::Factory( childKey, childName, this );
  this->RegisterGroup<EventBase>( childName, std::move(event) );
}


void EventManager::Run(dataRepository::ManagedGroup * domain)
{

  MeshLevel * const meshLevel = domain->group_cast<DomainPartition*>()->getMeshBody(0)->getMeshLevel(0);

  CellBlockSubRegion * const
  subRegion = meshLevel->getElemManager()->GetRegion(0)->GetSubRegion(0);

  FixedOneToManyRelation & elemsToNodes = subRegion->nodeList();
  elemsToNodes.data();
//  elemsToNodes[elemIndex][localNodeIndex];

  FaceManager * const faceManager = meshLevel->getFaceManager();
  OrderedVariableOneToManyRelation & facesToNodes = faceManager->nodeList();
//  facesToNodes[faceIndex][localNodeIndex];
  faceManager->m_isExternal;

  NodeManager * const nodeManager = meshLevel->getNodeManager();
  array1d< R1Tensor > & X = nodeManager->referencePosition();
  array1d< R1Tensor > & u = nodeManager->getReference< array1d< R1Tensor > >("TotalDisplacement");
  array1d< R1Tensor > & a = nodeManager->getReference< array1d< R1Tensor > >( keys::Acceleration );
  array1d< real64 > & nodalMass = nodeManager->getReference< array1d< real64 > >( keys::Mass );



  real64& time = *(this->getData<real64>(viewKeys.time));
  real64& dt = *(this->getData<real64>(viewKeys.dt));
  integer& cycle = *(this->getData<integer>(viewKeys.cycle));
  
  real64 const maxTime = this->getReference<real64>(viewKeys.maxTime);
  integer const maxCycle = this->getReference<integer>(viewKeys.maxCycle);
  integer const verbosity = this->getReference<integer>(viewKeys.verbosity);
  integer exitFlag = 0;




  // Setup event targets
  this->forSubGroups<EventBase>([]( EventBase * subEvent ) -> void
  {
    subEvent->GetTargetReferences();
  });

  // Run problem
  real64 epsilon = std::numeric_limits<real64>::epsilon();
  while(((maxTime < 0) || (maxTime - time > epsilon)) && ((maxCycle < 0) || (cycle < maxCycle)) && (exitFlag == 0))
  {
    real64 nextDt = 1e6;
    std::cout << "Time: " << time << "s, dt:" << dt << "s, Cycle: " << cycle << std::endl;

    this->forSubGroups<EventBase>([&]( EventBase * subEvent ) -> void
    {
      subEvent->CheckEvents(time, dt, cycle, domain);
      integer eventForecast = subEvent->GetForecast();

      if (eventForecast == 1)
      {
        subEvent->SignalToPrepareForExecution(time, dt, cycle, domain);
      }

      if (eventForecast <= 0)
      {
        subEvent->Execute(time, dt, cycle, domain);
      }

      real64 requestedDt = 1e6;
      if (eventForecast <= 1)
      {
        requestedDt = subEvent->GetTimestepRequest(time + dt);
      }
      nextDt = std::min(requestedDt, nextDt);

      exitFlag += subEvent->GetExitFlag();

      // Debug information
      if (verbosity > 0)
      {
        std::cout << "     Event: " << subEvent->getName() << ", f=" << eventForecast << ", dt_r=" << requestedDt << std::endl;
      }      
    });

    time += dt;
    ++cycle;
    dt = nextDt;
    dt = (maxTime - time < dt) ? (maxTime - time) : dt;
  }
}

} /* namespace geosx */
