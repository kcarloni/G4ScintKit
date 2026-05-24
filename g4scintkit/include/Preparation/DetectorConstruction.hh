/*
 * author:      Erik Dietz-Laursonn
 * institution: Physics Institute 3A, RWTH Aachen University, Aachen, Germany
 * copyright:   Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License
 */


#ifndef DETECTORCONSTRUCTION_HH_
#define DETECTORCONSTRUCTION_HH_

#include <G4VUserDetectorConstruction.hh>
#include <G4ThreeVector.hh>
#include <G4Transform3D.hh>
#include <G4Box.hh>
#include <G4Tubs.hh>
#include <G4Cons.hh>
#include <G4IntersectionSolid.hh>
#include <G4SubtractionSolid.hh>
#include <G4LogicalVolume.hh>
#include <G4Material.hh>
#include <G4OpticalSurface.hh>

#include <SimulationMessenger.hh>
#include <GoddessProperties.hh>
#include <ScintillatorTileConstructor.hh>
#include <FibreConstructor.hh>
#include <PhotonDetectorConstructor.hh>
#include <OpticalCouplingConstructor.hh>

#include <GeometryManifest.hh>

#ifdef USE_G4SIPM
#include "G4Sipm.hh"
#include "housing/G4SipmHousing.hh"
#include "model/G4SipmModel.hh"
#include "model/G4SipmModelFactory.hh"
#endif


// class variables begin with capital letters, local variables with small letters



/// <b> (Part of the example simulation and not belonging to GODDeSS.) </b> Builds the simulated setup, using functions inherited from G4VUserDetectorConstruction.
class DetectorConstruction: public G4VUserDetectorConstruction
{
public:

	/**
	*  Constructor:
	*  - sets class variables to default values
	*/
	/**< class storing and providing variables which are needed in different parts of the simulation. */
	DetectorConstruction( SimulationMessenger * simulationMessenger )
	// initializer list -- order set by class definition below
	: Messenger(simulationMessenger)
	, PropertyTools(Messenger->GetGoddessMessenger()->GetPropertyToolsManager())
	, SearchOverlaps(Messenger->GetSearchOverlaps())
	, STConstructor(Messenger->GetGoddessMessenger()->GetScintillatorTileConstructor())
	, FConstructor(Messenger->GetGoddessMessenger()->GetFibreConstructor())
	, PDConstructor(Messenger->GetGoddessMessenger()->GetPhotonDetectorConstructor())
	, OCConstructor(Messenger->GetGoddessMessenger()->GetOpticalCouplingConstructor())
	, Material_Vacuum(0)
	, Material_Air(0)
	, Material_Aluminum(0)
	, Material_Lead(0)
	, WorldDimensions(G4ThreeVector(NAN, NAN, NAN))
	, FibreStartPoint(G4ThreeVector(NAN, NAN, NAN))
	, FibreEndPoint(G4ThreeVector(NAN, NAN, NAN))
	{
	}

	/**
	 *  Destructor:
	 *  - deletes the objects, that have been created and will NOT automatically be created by Geant4
	 */
	~DetectorConstruction()
	{
		CleanUp();
		DeleteMaterialPropertiesTables();
		DeleteMaterials();
	}



	// the following function will be called by Geant4 in the initialisation process:
	G4VPhysicalVolume* Construct();

private:
	void DefineVariables();
	void DefineElements();
	void DefineMaterials();
	void DefineMaterialProperties();

	// Construct a photon detector (GODDESS G4PhotonDetector or g4sipm G4SipmHousing)
	// and its optical coupling to the fiber.
	// In GODDESS mode, uses OCConstructor to build the coupling slab.
	// In g4sipm mode, the housing's built-in epoxy window serves as optical coupling.
	G4VPhysicalVolume* ConstructSiPM(
		const G4String& name,
		G4VPhysicalVolume* refVolume,
		const G4ThreeVector& faceDir,
		const G4ThreeVector& relPos,
		G4double edgeLength,
		G4VPhysicalVolume* worldPhysical,
		G4VPhysicalVolume* fiberPhysical,
		const G4ThreeVector& couplingNormal,
		const G4ThreeVector& couplingPos,
		G4double couplingWidth,
		G4bool fiberIsBase = true);

	void CleanUp();
	void DeleteMaterials();
	void DeleteMaterialPropertiesTables();

	void PlaceManifest(const GeometryManifest& manifest,
	                   G4VPhysicalVolume* world_physical,
	                   G4LogicalVolume* world_logical);
	void PlaceCasing(const CasingSpec& casing, G4LogicalVolume* world_logical);


	SimulationMessenger * Messenger;
	PropertyToolsManager * PropertyTools;
	G4bool SearchOverlaps;
	ScintillatorTileConstructor * STConstructor;
	FibreConstructor * FConstructor;
	PhotonDetectorConstructor * PDConstructor;
	OpticalCouplingConstructor * OCConstructor;

	// Materials
	G4Material * Material_Vacuum;
	G4Material * Material_Air;
	G4Material * Material_Aluminum;
	G4Material * Material_Lead;

	// the world
	G4ThreeVector WorldDimensions;

	G4ThreeVector FibreStartPoint;
	G4ThreeVector FibreEndPoint;
	G4double FibreEndReflectivity;
};

#endif
