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
 * @file PoreVolumeCompressibleSolid.hpp
 */

#ifndef SRC_COMPONENTS_CORE_SRC_CONSTITUTIVE_POREVOLUMECOMPRESSIBLESOLID_HPP_
#define SRC_COMPONENTS_CORE_SRC_CONSTITUTIVE_POREVOLUMECOMPRESSIBLESOLID_HPP_

#include "constitutive/ConstitutiveBase.hpp"

#include "constitutive/ExponentialRelation.hpp"

namespace geosx
{
namespace dataRepository
{
namespace keys
{
string const poreVolumeCompressibleSolid = "PoreVolumeCompressibleSolid";
}
}

namespace constitutive
{


class PoreVolumeCompressibleSolid : public ConstitutiveBase
{
public:
  PoreVolumeCompressibleSolid( std::string const & name, ManagedGroup * const parent );

  virtual ~PoreVolumeCompressibleSolid() override;

  std::unique_ptr<ConstitutiveBase> DeliverClone( string const & name,
                                                  ManagedGroup * const parent ) const override;

  virtual void AllocateConstitutiveData( dataRepository::ManagedGroup * const parent,
                                         localIndex const numConstitutivePointsPerParentIndex ) override;


  static std::string CatalogName() { return dataRepository::keys::poreVolumeCompressibleSolid; }

  virtual string GetCatalogName() override { return CatalogName(); }

  virtual void StateUpdate( dataRepository::ManagedGroup const * const input,
                            dataRepository::ManagedGroup const * const parameters,
                            dataRepository::ManagedGroup * const stateVariables,
                            integer const systemAssembleFlag ) const override final {}

  virtual void PoreVolumeMultiplierCompute(real64 const & pres,
                                           localIndex const i,
                                           real64 & poro,
                                           real64 & dPVMult_dPres) override final;

  virtual void PressureUpdatePoint(real64 const & pres,
                                   localIndex const k,
                                   localIndex const q) override final;

  virtual void FillDocumentationNode() override;

  virtual void ReadXML_PostProcess() override;

  virtual void FinalInitialization( ManagedGroup * const parent ) override final;

  struct viewKeyStruct : public ConstitutiveBase::viewKeyStruct
  {
    dataRepository::ViewKey compressibility   = { "compressibility"   };
    dataRepository::ViewKey referencePressure = { "referencePressure" };
  } viewKeys;


private:

  /// scalar compressibility parameter
  real64 m_compressibility;

  /// reference pressure parameter
  real64 m_referencePressure;

  array2d<real64> m_poreVolumeMultiplier;
  array2d<real64> m_dPVMult_dPressure;

  ExponentialRelation<localIndex, real64> m_poreVolumeRelation;
};

inline void PoreVolumeCompressibleSolid::PressureUpdatePoint(real64 const & pres,
                                                             localIndex const k,
                                                             localIndex const q)
{
  m_poreVolumeRelation.Compute( pres, m_poreVolumeMultiplier[k][q], m_dPVMult_dPressure[k][q] );
}

}/* namespace constitutive */

} /* namespace geosx */


#endif //SRC_COMPONENTS_CORE_SRC_CONSTITUTIVE_POREVOLUMECOMPRESSIBLESOLID_HPP_