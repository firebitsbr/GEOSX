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
 * GEOSX is a free software; you can redistrubute it and/or modify it under
 * the terms of the GNU Lesser General Public Liscense (as published by the
 * Free Software Foundation) version 2.1 dated February 1999.
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

/**
 * @file ContactManagerBaseT.h
 * @author Scott Johnson
 * @date July 14, 2011
 */
#ifndef CONTACTMANAGERBASET_H_
#define CONTACTMANAGERBASET_H_

#include "../../dataRepository/Group.hpp"
#include "Common/Common.h"
//#include "DataStructures/VectorFields/ObjectDataStructureBaseT.h"
#include "Constitutive/Interface/InterfaceFactory.h"

/**
 * @author Scott Johnson
 * @brief Class to manage collections of face-face contacts
 */
class ContactManagerBaseT : public ObjectDataStructureBaseT
{
public:
  /**
   * @brief Contact manager constructor
   * @author Scott Johnson
   */
  ContactManagerBaseT();
  ContactManagerBaseT( const ObjectType objectType );

  void Initialize(   ){}

  void erase( const localIndex i );
  globalIndex resize( const localIndex size, const bool assignGlobals = false );

  /**
   * @brief Contact manager destructor
   * @author Scott Johnson
   */
  virtual ~ContactManagerBaseT();

  virtual void WriteSilo( SiloFile& siloFile,
                          const std::string& siloDirName,
                          const std::string& meshname,
                          const int centering,
                          const int cycleNum,
                          const realT problemTime,
                          const bool isRestart,
                          const std::string& regionName = "none",
                          const lArray1d& mask = lArray1d());

  virtual void ReadSilo( const SiloFile& siloFile,
                         const std::string& siloDirName,
                         const std::string& meshname,
                         const int centering,
                         const int cycleNum,
                         const realT problemTime,
                         const bool isRestart,
                         const std::string& regionName = "none",
                         const lArray1d& mask  = lArray1d());

  void ReadXML(TICPP::HierarchicalDataNode* hdn);

  void SetDomainBoundaryObjects( const ObjectDataStructureBaseT* const referenceObject  = NULL )
  { (void)referenceObject; }

  void SetIsExternal( const ObjectDataStructureBaseT* const referenceObject  = NULL )
  { (void)referenceObject; }

  void ExtractMapFromObjectForAssignGlobalObjectNumbers( const ObjectDataStructureBaseT& compositionObjectManager,
                                                         array<gArray1d>& objectToCompositionObject  )
  {
    (void)compositionObjectManager;
    (void)objectToCompositionObject;
    throw GPException("ContactManagerBaseT::ExtractMapFromObjectForAssignGlobalObjectNumbers() shouldn't be called\n");
  }

  int GetIndex(const localIndex body1, const localIndex body2) const;

  //TODO: feed in an array of valid entries so you don't need to check each
  // entry's size
  //better: take out size==0 entries and use an array with the face index
  // associated with each (now non-0 sized) array in the collection
  size_t Update(const array< lArray1d >& neighborList);

  void WriteVTKCellData(std::ofstream& out);

protected:
  virtual globalIndex insert( const localIndex i, const bool assignGlobals = false );

  /** @brief Recursive function called by BinarySearch
   * @param[in] value Value for which the index is to be found
   * @param[in] array Sorted (ascending) list of values to search
   * @param[in,out] imin Minimum index of the current search range
   * @param[in,out] imax Maximum index of the current search range
   * @return Index of value if gte 0 or if lt 0 then -1 if not yet found and -2
   * if not in array
   */
  template<class T>
  static inline int BinarySearchRecursive(const T value, const T* const array, int& imin, int& imax)
  {
    //you have located the spot, so is it there?
    if(imax == imin)
      return array[imax] == value ? imax : -2;

    //get mid-point
    int ii = (int)((imax - imin)/2.) + imin;
    if(value > array[ii])
    {
      //if greater, truncate search range to upper half
      imin = ii+1;
      return -1;
    }
    else if(value < array[ii])
    {
      //if less, truncate search range to lower half
      imax = ii-1;
      return -1;
    }
    else
    {
      //otherwise, must be equal
      return ii;
    }
  }

  /** @brief Binary search
   * @param[in] value Value for which the index is to be found
   * @param[in] array Sorted (ascending) list of values to search
   * @param[in] imin Search begin index
   * @param[in] imax Search end index
   * @return Index of value if gte 0 or if not found -1
   */
  template<class T>
  static inline int BinarySearch(const T value, const T* const array, const int imin, const int imax)
  {
    int result = -1, iimin = imin, iimax = imax;
    while(result == -1)
      result = BinarySearchRecursive<T>(value, array, iimin, iimax);
    return result < 0 ? -1 : result;
  }

public:
  ///contact states
  InterfaceBase* m_contact;
//  @annavarapusr1: Interfacial contact specific quantities
  int m_sliding_law;
  realT m_yield_traction;
  realT m_coulomb_coefficient;
  realT m_glob_penalty_n;
  realT m_glob_penalty_t1;
  realT m_glob_penalty_t2;
  realT m_stab_magnifying_factor;
  bool m_nitsche_active;
  bool m_nitsche_symmetry_active;
  bool m_implicitContactActive;
  bool m_weighted_nitsche_active;
  bool m_nitsche_normal_active;
  bool m_nitsche_tau1_active;
  bool m_nitsche_tau2_active;
  bool m_use_contact_search;
  realT m_traction_n_tol;

};

#endif /* CONTACTMANAGERT_H_ */
