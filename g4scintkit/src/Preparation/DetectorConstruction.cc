/*
 * author:      Erik Dietz-Laursonn
 * institution: Physics Institute 3A, RWTH Aachen University, Aachen, Germany
 * copyright:   Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License
 */


#include <G4VisAttributes.hh>
#include <G4PVPlacement.hh>
#include <G4GeometryManager.hh>
#include <G4LogicalSkinSurface.hh>
#include <G4LogicalBorderSurface.hh>
#include <G4PhysicalVolumeStore.hh>
#include <G4LogicalVolumeStore.hh>
#include <G4SolidStore.hh>
#include <G4Element.hh>
#include <G4MaterialTable.hh>

#include "DetectorConstruction.hh"
#include "ManifestFile.hh"

#include <map>
#include <set>
#include <stdexcept>
#include <string>

using namespace CLHEP;   //for mathematics (e.g. CLHEP::sqrt, CLHEP::pi, CLHEP::c_light, CLHEP::h_Planck,...)


// ---- helpers for the manifest seam (file-local) ----
namespace
{
	// Rebuild a G4Transform3D from a row-major rotation array + translation.
	G4Transform3D makeTransform(const double r[9], const G4ThreeVector& pos)
	{
		G4RotationMatrix rot( G4ThreeVector(r[0], r[3], r[6]),    // column X
		                      G4ThreeVector(r[1], r[4], r[7]),    // column Y
		                      G4ThreeVector(r[2], r[5], r[8]) );  // column Z
		return G4Transform3D(rot, pos);
	}

	G4VPhysicalVolume* lookupVolume(
			const std::map<G4String, G4VPhysicalVolume*>& volumes, const G4String& key)
	{
		std::map<G4String, G4VPhysicalVolume*>::const_iterator it = volumes.find(key);
		if (it == volumes.end())
			throw std::runtime_error("PlaceManifest: unknown volume reference '"
			        + std::string(key) + "'");
		return it->second;
	}
}



// class variables begin with capital letters, local variables with small letters



/**
 *  Function to create the simulation setup and register it to Geant4:
 *  - will be called by Geant4 in the initialisation process and <b> has to create materials and volumes as well as to return the pointer to the world volume (the volume that contains all other volumes) </b>
 *  - calls DefineVariables(), DefineElements(), DefineMaterials(), and DefineMaterialProperties().
 *  - creates the setup that is defined inside and registers it to Geant4
 */
G4VPhysicalVolume* DetectorConstruction::Construct()
{
	DefineVariables();
	DefineElements();
	DefineMaterials();
	DefineMaterialProperties();

	//   ######  solids (dimensions):  ######   //

		//---------- experimental hall (world volume) ----------//
		G4Box * world_solid = new G4Box("world_solid", 
			WorldDimensions.x(), WorldDimensions.y(), WorldDimensions.z());

	//

	//   ######  logical volumes (material):  ######   //

		//---------- experimental hall (world volume) ----------//
		G4LogicalVolume * world_logical = new G4LogicalVolume(world_solid, Material_Vacuum, "world_logical", 0, 0, 0);
		world_logical->SetVisAttributes(G4VisAttributes::Invisible);

		
		//   ######  physical volumes (placement):  ######   //
		// NOTE: G4PVPlacement puts the volume to be placed into EVERY physical volume emanating from the same logical volume 
		// (no matter whether the logical or physical volume is specified as mother volume)!
		// => If G4PVPlacement should be able to distinguish physical volumes, for each physical volume a separate logical volume has to be created.
		// => rule of thumb: For each volume that might become a mother volume, a separate logical volume should to be created.

		//---------- experimental hall (world volume) ----------//
		G4VPhysicalVolume * world_physical = new G4PVPlacement(G4Transform3D(), world_logical, "world", 0, false, 0);
	//

	// ===== geometry manifest =====
	G4String manifestInput = Messenger->GetManifestInputFile();
	if (manifestInput.empty())
		throw std::runtime_error(
			"DetectorConstruction: --manifest is required "
			"(named --setup geometries have been removed)");
	G4cout << "DetectorConstruction: reading geometry manifest " << manifestInput << G4endl;
	GeometryManifest manifest = ManifestFile::Read(manifestInput);
	PlaceManifest(manifest, world_physical, world_logical);

	G4String dumpPath = Messenger->GetManifestDumpFile();
	if ( !dumpPath.empty() )
	{
		ManifestFile::Write( manifest, dumpPath );
		G4cout << "DetectorConstruction: wrote geometry manifest "
		       << dumpPath << G4endl;
	}

	return world_physical;   // experimental hall with all volumes placed inside
}


/**
 *  Generic interpreter: build the Geant4 geometry described by `manifest` by
 *  calling the GODDESS primitive constructors. Volumes are resolved by name.
 *
 *  Elements are placed in EXACTLY the manifest's placement order, which is
 *  geometry-critical: GODDESS ConstructWrapping (with no explicit cut-list)
 *  subtracts every physical volume that exists at call time, so a wrapping must
 *  be replayed at the precise point in the sequence where it was emitted. The
 *  per-element setter calls replay exactly what each manifest entry records, so
 *  any GODDESS setter state is reproduced faithfully. The casing is placed last.
 */
void DetectorConstruction::PlaceManifest(const GeometryManifest& manifest,
        G4VPhysicalVolume* world_physical, G4LogicalVolume* world_logical)
{
	std::map<G4String, G4VPhysicalVolume*>  physicalOf;
	std::map<G4String, G4ScintillatorTile*> tileOf;
	physicalOf["world"] = world_physical;

	for (size_t i = 0; i < manifest.placements.size(); ++i)
	{
		const PlacementEntry& p = manifest.placements[i];
		switch (p.kind)
		{
		case PlacementEntry::SCINT:
		{
			const ScintEntry& s = p.scint;
			if (!s.g4name.empty()) STConstructor->SetScintillatorName(s.g4name);
			STConstructor->SetScintillatorTransformation(makeTransform(s.rot, s.pos));
			if (s.sensitive) STConstructor->ConstructASensitiveDetector();

			G4ScintillatorTile* tile = STConstructor->ConstructScintillator(
				s.dims, s.material_file, lookupVolume(physicalOf, s.mother));

			tileOf[s.name]     = tile;
			physicalOf[s.name] = tile->GetScintillator_physicalVolume();
			break;
		}
		case PlacementEntry::FIBER:
		{
			const FiberEntry& f = p.fiber;
			if (f.glued)
				FConstructor->SetFibreGlued(f.glue_file, f.glue_profile);
			if (!f.reference.empty())
				FConstructor->SetFibreReferenceVolume(lookupVolume(physicalOf, f.reference));
			if (!std::isnan(f.start_reflectivity))
				FConstructor->SetFibreStartPointReflectivity(f.start_reflectivity);
			if (!std::isnan(f.end_reflectivity))
				FConstructor->SetFibreEndPointReflectivity(f.end_reflectivity);

			G4VPhysicalVolume* mother = lookupVolume(physicalOf, f.mother);
			G4Fibre* fibre = 0;
			if (f.kind == "bent")
				fibre = FConstructor->ConstructFibre(
					f.material_file, mother, f.start, f.end, f.bend_angle, f.bend_axis);
			else
				fibre = FConstructor->ConstructFibre(
					f.material_file, mother, f.start, f.end);

			physicalOf[f.name] = fibre->GetOutermostVolumeOutsideMother_physicalVolume();
			break;
		}
		case PlacementEntry::WRAP:
		{
			const WrapEntry& w = p.wrap;
			if (!w.g4name.empty()) STConstructor->SetWrappingName(w.g4name);

			// Wrapping cut-volume candidates. An explicit w.cut is used
			// verbatim (manual override); otherwise the candidates are
			// auto-derived from the manifest reference graph in two passes:
			//   pass 1 — every earlier fibre that belongs to this tile (its
			//            mother or reference is the wrapped scintillator);
			//   pass 2 — every earlier fibre sharing a loop_id with one from
			//            pass 1 (multi-segment fibres routed through this
			//            tile: the in-scint piece(s) live in the scint frame
			//            but the cut that actually punches through the wrap
			//            shell is the sibling piece in the wrap's mother).
			// GODDESS ConstructWrapping then geometrically filters the
			// candidates down to those that actually penetrate the wrapping
			// shell — and silently rejects candidates whose Geant4 mother
			// differs from the wrap's mother, so pass 1's in-scint pieces
			// are harmless extras and pass 2's world-frame siblings are the
			// ones that actually create the through-holes.
			std::vector<G4VPhysicalVolume*> cuts;
			if (!w.cut.empty())
			{
				for (size_t j = 0; j < w.cut.size(); ++j)
					cuts.push_back(lookupVolume(physicalOf, w.cut[j]));
			}
			else
			{
				std::set<G4int> loop_ids;               // pass-1 loop_ids
				std::set<std::string> picked;           // pass-1 fibre names
				for (size_t j = 0; j < i; ++j)
				{
					if (manifest.placements[j].kind != PlacementEntry::FIBER)
						continue;
					const FiberEntry& cf = manifest.placements[j].fiber;
					if (cf.mother == w.scint || cf.reference == w.scint)
					{
						cuts.push_back(lookupVolume(physicalOf, cf.name));
						picked.insert(cf.name);
						if (cf.loop_id >= 0) loop_ids.insert(cf.loop_id);
					}
				}
				if (!loop_ids.empty())
				{
					for (size_t j = 0; j < i; ++j)
					{
						if (manifest.placements[j].kind != PlacementEntry::FIBER)
							continue;
						const FiberEntry& cf = manifest.placements[j].fiber;
						if (cf.loop_id < 0) continue;
						if (picked.count(cf.name)) continue;
						if (loop_ids.count(cf.loop_id))
							cuts.push_back(lookupVolume(physicalOf, cf.name));
					}
				}
			}
			STConstructor->SetWrappingCutVolumes(cuts);
			std::map<G4String, G4ScintillatorTile*>::const_iterator it =
				tileOf.find(w.scint);
			if (it == tileOf.end())
				throw std::runtime_error(
					"PlaceManifest: WRAP references unknown scintillator '"
					+ std::string(w.scint) + "'");
			STConstructor->ConstructWrapping(it->second, w.material_file);
			break;
		}
		case PlacementEntry::SIPM:
		{
			const SipmEntry& sp = p.sipm;
			ConstructSiPM(
				sp.name,
				lookupVolume(physicalOf, sp.ref_volume),
				sp.face_dir,
				sp.rel_pos,
				sp.edge_length,

				world_physical,
				lookupVolume(physicalOf, sp.fiber),
				sp.coupling_normal,
				sp.coupling_pos,
				sp.coupling_width,
				sp.fiber_is_base,
				sp.model);
			break;
		}
		}
	}

	// ---- casing (always last) ----
	PlaceCasing(manifest.casing, world_logical);
}


/**
 *  Place the outer aluminum box and lead sheet from a CasingSpec. Verbatim port
 *  of the legacy casing code, sourcing its inputs from the manifest.
 */
void DetectorConstruction::PlaceCasing(const CasingSpec& c, G4LogicalVolume* world_logical)
{
	G4double al_box_top_y = c.module_max_y;   // default if no aluminum

	// add aluminum box surrounding the scintillator bars (with 1mm air gap)
	if ( c.aluminum_thickness > 0.0 * CLHEP::mm )
	{
		G4double al_gap = 1.0 * CLHEP::mm;
		G4double al_t   = c.aluminum_thickness;

		G4double inner_half_x = c.module_half_x + al_gap;
		G4double inner_half_y = (c.module_max_y - c.module_min_y) / 2 + al_gap;
		G4double inner_half_z = c.module_half_z + al_gap;

		G4double outer_half_x = inner_half_x + al_t;
		G4double outer_half_y = inner_half_y + al_t;
		G4double outer_half_z = inner_half_z + al_t;

		G4double box_center_y = (c.module_min_y + c.module_max_y) / 2;
		al_box_top_y = box_center_y + outer_half_y;

		G4Box* al_outer = new G4Box("al_outer", outer_half_x, outer_half_y, outer_half_z);
		G4Box* al_inner = new G4Box("al_inner", inner_half_x, inner_half_y, inner_half_z);
		G4SubtractionSolid* al_box_solid =
			new G4SubtractionSolid("al_box_solid", al_outer, al_inner);

		G4LogicalVolume* al_box_log =
			new G4LogicalVolume(al_box_solid, Material_Aluminum, "Al_box_log");
		al_box_log->SetVisAttributes(G4Colour::Blue());

		new G4PVPlacement(0, G4ThreeVector(0, box_center_y, 0), al_box_log,
		                  "aluminum_box_phys", world_logical, false, 0);
	}

	// add lead sheet on top of the aluminum box
	if ( c.lead_thickness > 0.0 * CLHEP::mm )
	{
		G4double lead_half_thickness = c.lead_thickness / 2;

		G4double sheet_half_x = c.num_bars * c.bar_width;
		G4double sheet_half_z = c.scinti_z / 2;
		G4double sheet_pos_y  = al_box_top_y + lead_half_thickness;

		G4ThreeVector sheet_pos(0 * CLHEP::mm, sheet_pos_y, 0 * CLHEP::mm);

		G4Box* lead_sheet = new G4Box("lead_sheet",
			sheet_half_x, lead_half_thickness, sheet_half_z);

		G4LogicalVolume* lead_sheet_log =
			new G4LogicalVolume(lead_sheet, Material_Lead, "Pb_sheet_log");
		lead_sheet_log->SetVisAttributes(G4Colour::Grey());

		new G4PVPlacement(0, sheet_pos, lead_sheet_log,
		                  "lead_sheet_phys", world_logical, false, 0);
	}
}



/**
*  Function for the initialisation of variables.
*/
void DetectorConstruction::DefineVariables()
{
	//---------- experimental hall (world volume) ----------//
		WorldDimensions = G4ThreeVector(10. * m, 10. * m, 10. * m);
	//

	//---------- fibre ----------//
		FibreEndReflectivity = 0.9;
	//
}


/**
*  Function to create chemical elements and register them to Geant4:
*  - creates hydrogen, carbon, nitrogen, oxygen, fluorine, aluminum, titanium, and lead
*  - <b> if other chemical elements are needed for the simulation, they have to be defined here </b>
*/
void DetectorConstruction::DefineElements()
{
// http://pdg.lbl.gov/2009/AtomicNuclearProperties/index.html
	new G4Element("Hydrogen", "H", 1., 1.00794 * g/mole);
	new G4Element("Carbon", "C", 6., 12.0107 * g/mole);
	new G4Element("Nitrogen", "N", 7., 14.0067 * g/mole);
	new G4Element("Oxygen", "O", 8., 15.9994 * g/mole);
	new G4Element("Fluorine", "F", 9., 18.9984032 * g/mole);
	new G4Element("Aluminum", "Al", 13., 26.9815386 * g/mole);
	new G4Element("Silicon", "Si", 14., 28.0855 * g/mole);
	new G4Element("Titanium", "Ti", 22., 47.867 * g/mole);
	new G4Element("Lead", "Pb", 82., 207.2 * g/mole);
}

/**
*  Function to create materials and register them to Geant4.
*/
void DetectorConstruction::DefineMaterials()
{
	//---------- vacuum ----------//
	// G4Material(const G4String &name, G4double z, G4double a, G4double density, G4State state=kStateUndefined, G4double temp=STP_Temperature, G4double pressure=STP_Pressure)
	Material_Vacuum = new G4Material("Vacuum", 1., 
		1.01 * g/mole, universe_mean_density, 
		kStateGas, 0.1 * kelvin, 1.e-19 * pascal
	);		// definition from Geant

	//---------- air ----------//
	// G4Material(const G4String &name, G4double density, G4int nComponents, G4State state=kStateUndefined, G4double temp=STP_Temperature, G4double pressure=STP_Pressure)
	Material_Air = new G4Material("Air", 1.293 * kg/m3, 2);
	Material_Air->AddElement(G4Element::GetElement("Nitrogen"), 70 * perCent);
	Material_Air->AddElement(G4Element::GetElement("Oxygen"), 30 * perCent);

	//--------- aluminum ------//
	G4double aluminum_density = 2.699 * g/cm3;
	G4double aluminum_a = 26.98 * g/mole;
	Material_Aluminum = new G4Material("metalAluminum", 13, aluminum_a, aluminum_density, kStateSolid);

	//--------- lead ----------//
	G4double lead_density = 11.35 * g/cm3;
	G4double lead_a       = 207.2 * g/mole;  // atomic mass
	Material_Lead = new G4Material("metalLead", 82, lead_a, lead_density, kStateSolid);

}

//   ######  material properties:  ######   //
// NOTE: possible Properties:
	//         "RINDEX":			(spectrum (in dependence of the photon energy))		(obligatory property!)
	//		defines the refraction index of the material, used for boundary processes, Cerenkov radiation and Rayleigh scattering
	//         "ABSLENGTH":			(spectrum (in dependence of the photon energy))
	//		defines the absorption length (absorption spectrum) of the material, used for the "normal" absorption of optical photons (default is infinity, i.e. no absorption)
	//		(the absorption length for the WLS process of WLS materials is specified by "WLSABSLENGTH", "ABSLENGTH" can be specified additionally to simulate a non-WLS absorption fraction)
	//         "RAYLEIGH":			(spectrum (in dependence of the photon energy))
	//		defines the absorption length of the material, used for the rayleigh scattering of optical photons (default is infinity, i.e. no scattering)
	//
	//         "SCINTILLATIONYIELD":	(constant value (energy independent))			(obligatory property for scintillator materials!)
	//		defines the mean number of photons, emitted per MeV energy deposition in the scintillator material (the real number is Poisson/Gauss distributed)
	//		(can also be specified separately for different particles by putting "ELECTRON...", "PROTON...", "DEUTERON...", "TRITON...", "ALPHA...", "ION..." infront of "SCINTILLATIONYIELD")
	//		(default is 0, i.e. no scintillation process)
	//         "RESOLUTIONSCALE":		(constant value (energy independent))
	//		defines the intrinsic resolution of the scintillator material, used for the statistical distribution of the number of generated photons in the scintillation process
	//		(values > 1 result in a wider distribution, values < 1 result in a narrower distribution -> 1 should be chosen as default)
	//		(default is 0)
	//         "FASTCOMPONENT":		(spectrum (in dependence of the photon energy))		(at least one "...COMPONENT" is obligatory for scintillator materials!)
	//		defines the emission spectrum of the material, used for the fast scintillation process	NOTE: emission spectra are NOT linearly extrapolated between two given points!
	//         "SLOWCOMPONENT":		(spectrum (in dependence of the photon energy))		(at least one "...COMPONENT" is obligatory for scintillator materials!)
	//		defines the emission spectrum of the material, used for the slow scintillation process	NOTE: emission spectra are NOT linearly extrapolated between two given points!
	//         "FASTTIMECONSTANT":		(constant value (energy independent))
	//		defines the decay time (time between energy deposition and photon emission), used for the fast scintillation process (default is 0)
	//         "SLOWTIMECONSTANT":		(constant value (energy independent))
	//		defines the decay time (time between energy deposition and photon emission), used for the slow scintillation process (default is 0)
	//         "FASTSCINTILLATIONRISETIME":	(constant value (energy independent))
	//		defines the rise time (time between the start of the emission and the emission peak), used for the fast scintillation process (default is 0)
	//         "SLOWSCINTILLATIONRISETIME":	(constant value (energy independent))
	//		defines the rise time (time between the start of the emission and the emission peak), used for the slow scintillation process (default is 0)
	//         "YIELDRATIO":		(constant value (energy independent))			(obligatory property for scintillator materials, if both "...COMPONENT"s are specified!)
	//		defines relative strength of the fast scintillation process as a fraction of total scintillation yield (default is 0)
	//
	//         "WLSABSLENGTH":		(spectrum (in dependence of the photon energy))		(obligatory property for WLS materials!)
	//		defines the absorption length (absorption spectrum) of the material, used for the WLS process (default is infinity, i.e. no WLS process)
	//         "WLSCOMPONENT":		(spectrum (in dependence of the photon energy))		(obligatory property for WLS materials!)
	//		defines the emission spectrum of the material, used for the WLS process	NOTE: emission spectra are NOT linearly extrapolated between two given points!
	//         "WLSTIMECONSTANT":		(constant value (energy independent))
	//		defines the decay time (time between absorption and emission), used for the WLS process (default is 0)
	//         "WLSMEANNUMBERPHOTONS":	(constant value (energy independent))
	//		defines the mean number of photons, emitted for each photon that was absorbed by the WLS material
	//		(if specified, the real number of emitted photons is Poisson distributed, else the real number of emitted photons is 1)
//

/**
 *  Function to set material properties and register them to Geant4.
 */
void DetectorConstruction::DefineMaterialProperties()
{
	//---------- experimental hall (world volume) -> vacuum or air ----------//
	// by now, GEANT expects that a material property with the name identifier RINDEX is wavelength/energy dependent (http://hypernews.slac.stanford.edu/HyperNews/geant4/get/opticalphotons/379.html)!
	G4MaterialPropertyVector * refractiveIndex_Vacuum = PropertyTools->GetPropertyDistribution(1.);

	G4MaterialPropertiesTable * mpt_Vacuum = new G4MaterialPropertiesTable();
	mpt_Vacuum->AddProperty("RINDEX", refractiveIndex_Vacuum);
	Material_Vacuum->SetMaterialPropertiesTable(mpt_Vacuum);


	// by now, GEANT expects that a material property with the name identifier RINDEX is wavelength/energy dependent (http://hypernews.slac.stanford.edu/HyperNews/geant4/get/opticalphotons/379.html)!
	G4MaterialPropertyVector * refractiveIndex_Air = PropertyTools->GetPropertyDistribution(1.);
	// the refractive index of air can approximated by 1 as it only varies from 1.000308 (230nm) to 1.00027417 (1000nm)
	// http://refractiveindex.info/?group=GASES&material=Air

	// absorption and rayleigh scattering can be neglected, as only relatively thin layers are used in this simulation

	G4MaterialPropertiesTable * mpt_Air = new G4MaterialPropertiesTable();
	mpt_Air->AddProperty("RINDEX", refractiveIndex_Air);
	Material_Air->SetMaterialPropertiesTable(mpt_Air);
}


void DetectorConstruction::CleanUp()
{
	G4GeometryManager::GetInstance()->OpenGeometry();

	G4LogicalSkinSurface::CleanSurfaceTable();
	G4LogicalBorderSurface::CleanSurfaceTable();

	G4PhysicalVolumeStore::GetInstance()->Clean();
	G4LogicalVolumeStore::GetInstance()->Clean();
	G4SolidStore::GetInstance()->Clean();
}


/**
 *  Construct the photon detector and its optical coupling to the fiber.
 *
 *  Dispatch is per-SiPM via `model`: a non-empty alias selects a g4sipm
 *  SiPM model (via G4SipmModelFactory) and the housing's built-in epoxy
 *  window (n=1.5) serves as the optical interface — no external coupling
 *  slab is constructed. An empty `model` falls back to the GODDESS
 *  G4PhotonDetector path with an OCConstructor-built coupling slab.
 */
G4VPhysicalVolume* DetectorConstruction::ConstructSiPM(
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
	G4bool fiberIsBase,
	const G4String& model)
{
#ifdef USE_G4SIPM
	if (!model.empty())
	{
		// ---- g4sipm path ----
		G4SipmModel* g4sipmModel = nullptr;

		if (model == "generic")
		{
			g4sipmModel = G4SipmModelFactory::getInstance()->createGenericSipmModel();
		}
		else if (model == "hamamatsu-s10362-11-100c")
		{
			g4sipmModel = G4SipmModelFactory::getInstance()->createHamamatsuS1036211100();
		}
		else if (model == "hamamatsu-s10362-33-100c")
		{
			g4sipmModel = G4SipmModelFactory::getInstance()->createHamamatsuS1036233100();
		}
		else if (model == "hamamatsu-s10362-33-050c")
		{
			g4sipmModel = G4SipmModelFactory::getInstance()->createHamamatsuS1036233050();
		}
		else if (model == "hamamatsu-s12651-050")
		{
			g4sipmModel = G4SipmModelFactory::getInstance()->createHamamatsuS12651050();
		}
		else if (model == "hamamatsu-s12573-100c")
		{
			g4sipmModel = G4SipmModelFactory::getInstance()->createHamamatsuS12573100C();
		}
		else if (model == "hamamatsu-s12573-100x")
		{
			g4sipmModel = G4SipmModelFactory::getInstance()->createHamamatsuS12573100X();
		}
		else
		{
			throw std::runtime_error(
			    "ConstructSiPM: SiPM '" + std::string(name) + "' has unknown model '"
			    + std::string(model) + "' (extend the alias switch in "
			    "DetectorConstruction::ConstructSiPM, or use \"\" for GODDESS PD)");
		}

		G4Sipm* g4sipmDev = new G4Sipm(g4sipmModel);
		G4SipmHousing* housing = new G4SipmHousing(g4sipmDev);

		// Compute absolute world position.
		// relPos includes a coupling_width offset from the fiber end (in the -faceDir
		// direction) to leave room for the GODDESS coupling slab. Since g4sipm skips
		// the external coupling, compensate by shifting the housing toward the fiber
		// by coupling_width (i.e. add faceDir * couplingWidth).
		// The Dz/2 offset ensures the window face (not housing center) lands at the fiber.
		G4ThreeVector absPos = refVolume->GetObjectTranslation() + relPos
		                       - faceDir.unit() * (housing->getDz() / 2.0 - couplingWidth);

		// Compute rotation to align the housing's default face direction (+Z) with faceDir.
		// G4SipmHousing is built with its sensitive face pointing in the +Z direction.
		G4RotationMatrix* rot = new G4RotationMatrix();
		G4ThreeVector defaultFace(0., 0., 1.);
		G4ThreeVector axis = defaultFace.cross(faceDir);
		if (axis.mag() > 1e-6)
		{
			G4double angle = std::acos(defaultFace.dot(faceDir.unit()));
			rot->rotate(angle, axis.unit());
		}
		else if (defaultFace.dot(faceDir) < 0)
		{
			// Anti-parallel: rotate 180 deg around X
			rot->rotateX(180. * CLHEP::deg);
		}

		return housing->buildAndPlace(worldPhysical, absPos, rot);
	}
#endif

	// ---- GODDESS path (default) ----
	PDConstructor->SetPhotonDetectorName(name);
	PDConstructor->SetPhotonDetectorReferenceVolume(refVolume);
	PDConstructor->SetSensitiveSurfaceNormalRelativeToReferenceVolume(faceDir);
	PDConstructor->SetSensitiveSurfacePositionRelativeToReferenceVolume(relPos);
	G4PhotonDetector* pd = PDConstructor->ConstructPhotonDetector(edgeLength, worldPhysical);
	G4VPhysicalVolume* sipmPhysical = pd->GetCoating_physicalVolume();

	// ---- optical coupling (GODDESS only) ----
	OCConstructor->SetCouplingName(name + "_coupling");
	OCConstructor->SetCouplingSurfaceNormalRelativeToBaseVolume(couplingNormal);
	OCConstructor->SetCouplingCentrePositionRelativeToBaseVolume(couplingPos);
	if (fiberIsBase)
	{
		OCConstructor->CoupleVolumes(
			fiberPhysical, sipmPhysical,
			edgeLength, couplingWidth, worldPhysical);
	}
	else
	{
		OCConstructor->CoupleVolumes(
			sipmPhysical, fiberPhysical,
			edgeLength, couplingWidth, worldPhysical);
	}

	return sipmPhysical;
}
