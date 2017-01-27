/**
 * @file FaceManager.h
 * @author settgast1
 */

#ifndef FACEMANAGER_H_
#define FACEMANAGER_H_

#include "managers/ObjectManagerBase.hpp"

namespace geosx
{


class FaceManager: public ObjectManagerBase
{
public:

  /**
   * @name Static Factory Catalog Functions
   */
  ///@{

  static string CatalogName()
  {
    return "FaceManager";
  }

  string getName() const override final
  {
    return FaceManager::CatalogName();
  }


  ///@}
  ///
  ///
  FaceManager( string const & , ObjectManagerBase * const parent );
  virtual ~FaceManager();

  void Initialize(  ){}


//  void BuildFaces( const NodeManager& nodeManager, const ElementManager& elemManager );


public:



private:

};

}
#endif /* FACEMANAGERT_H_ */
