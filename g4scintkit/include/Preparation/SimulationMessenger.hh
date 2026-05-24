/*
 * author:      Erik Dietz-Laursonn
 * institution: Physics Institute 3A, RWTH Aachen University, Aachen, Germany
 * copyright:   Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License
 */


#ifndef SimulationMessenger_h
#define SimulationMessenger_h 1

#include <G4UImessenger.hh>
#include <G4UIdirectory.hh>
#include <G4UIcmdWithABool.hh>
#include <G4UIcmdWithAString.hh>
#include <UserRunInformation.hh>
#include <HDF5Writer.hh>

#include <GODDeSS_Messenger.hh>



// class variables begin with capital letters, local variables with small letters



/// <b> (Part of the example simulation and not belonging to GODDeSS.) </b> Class to store and provide variables which are needed in different parts of the simulation.
class SimulationMessenger: public G4UImessenger
{
public:

	/**
	 *  Constructor:
	 *  - sets class variables to default values
	 *  - creates commands for operating in the interactive mode
	 */
	SimulationMessenger( GODDeSS_Messenger * goddess_messenger   /**< pointer to the GODDeSS_DataStorage that is to be used */
			   )
	// initialising the variables (doing it with default values, "" or "0" is just to prevent errors from wrongly initialised variables), this has to be done in the order of their appearance in the hh-file:
	: GoddessMessenger(goddess_messenger)
	, UsePhotonListSource(false)
	, UseParticleListSource(false)
	, SearchOverlaps(false)
	, RunInformation(0)
	, DiffuserPropertyFile("")
	, TrackPhotons(true)
	, CurrentRunId(-1)
	, CurrentCsvEventId(-1)
	, Quiet(false)
	, UseG4Sipm(false)
	, SipmModelFile("")
	, GPSFile("inj.dat")
	, DataFile("events.dat")
	, ControlFile("control.dat")
	, HitFile("hits.dat")
	, H5Writer(new HDF5Writer())
	, HDF5File("")
	, ManifestInputFile("")
	, ManifestDumpFile("")
	{
		SimulationDir = new G4UIdirectory("/simulation/");
		SimulationDir->SetGuidance("UI commands of this setup");

		TrackPhotonsCmd = new G4UIcmdWithABool("/simulation/trackOpticalPhotons",this);
		TrackPhotonsCmd->SetGuidance("Switch optical photon tracking on/off. Default is \"true\" ");
		TrackPhotonsCmd->SetDefaultValue(true);

		OutputFileCmd = new G4UIcmdWithAString("/simulation/outFileName", this);
		OutputFileCmd->SetGuidance("Set the output file.");
		OutputFileCmd->SetDefaultValue("outputSimulation.txt");
	}

	/**
	 *  Destructor:
	 *  - deletes the command objects
	 */
	~SimulationMessenger()
	{
		H5Writer->finalize();
		delete H5Writer;
		delete OutputFileCmd;
		delete TrackPhotonsCmd;
		delete SimulationDir;
	}



	void SetNewValue(G4UIcommand*, G4String);

private:
	G4UIdirectory* SimulationDir;

	G4UIcmdWithABool* TrackPhotonsCmd;
	G4UIcmdWithAString* OutputFileCmd;

	GODDeSS_Messenger * GoddessMessenger;
	G4bool UsePhotonListSource;
	G4bool UseParticleListSource;
	G4bool SearchOverlaps;
	UserRunInformation * RunInformation;
	G4String DiffuserPropertyFile;
	G4bool TrackPhotons;
	G4int CurrentRunId;
	G4int CurrentCsvEventId;
	G4bool Quiet;
	G4bool UseG4Sipm;
	G4String SipmModelFile;
	G4String GPSFile;
	G4String DataFile;
	G4String ControlFile;
	G4String HitFile;
	HDF5Writer* H5Writer;
	G4String HDF5File;
	G4String ManifestInputFile;
	G4String ManifestDumpFile;

public:
	/**
	 *  @return pointer to the GODDeSS_DataStorage that is to be used
	 */
	GODDeSS_Messenger * GetGoddessMessenger() const
	{ return GoddessMessenger; }

	/**
	 *  Set whether a list of photons or the "normal" particle source is to be used for the simulation.
	 */
	void SetUsePhotonListSource(G4bool usePhotonListSource)
	{ UsePhotonListSource = usePhotonListSource; }
	/**
	 *  @return G4bool, whether a list of photons or the "normal" particle source is to be used for the simulation
	 */
	G4bool GetUsePhotonListSource() const
	{ return UsePhotonListSource; }

	void SetUseParticleListSource(G4bool useParticleListSource)
	{ UseParticleListSource = useParticleListSource; }
	G4bool GetUseParticleListSource() const
	{ return UseParticleListSource; }

	/**
	 *  Set whether Geant4 should search for overlaps when placing the physical volumes
	 */
	void SetSearchOverlaps(G4bool searchOverlaps)
	{ SearchOverlaps = searchOverlaps; }
	/**
	 *  @return G4bool, whether Geant4 should search for overlaps when placing the physical volumes
	 */
	G4bool GetSearchOverlaps() const
	{ return SearchOverlaps; }

	/**
	 *  Set the pointer to the UserRunInformation.
	 */
	void SetRunInformation(UserRunInformation * runInformation)
	{ RunInformation = runInformation; }
	/**
	 *  @return pointer to the UserRunInformation
	 */
	UserRunInformation * GetRunInformation() const
	{ return RunInformation; }

	/**
	 *  Set whether optical photons is to be tracked in the simulaton. (If "false", optical photons are killed in their first step.)
	 */
	void SetTrackPhotons(G4bool trackPhotons)
	{ TrackPhotons = trackPhotons; }
	/**
	 *  @return G4bool, whether optical photons is to be tracked in the simulaton (if "false", optical photons are killed in their first step)
	 */
	G4bool GetTrackPhotons() const
	{ return TrackPhotons; }

	void SetCurrentRunId(G4int runId)
	{ CurrentRunId = runId; }
	G4int GetCurrentRunId() const
	{ return CurrentRunId; }

	void SetCurrentCsvEventId(G4int eventId)
	{ CurrentCsvEventId = eventId; }
	G4int GetCurrentCsvEventId() const
	{ return CurrentCsvEventId; }

	void SetQuiet(G4bool quiet)
	{ Quiet = quiet; }
	G4bool GetQuiet() const
	{ return Quiet; }

	/**
	 *  Set path to the file containing the particle source's settings.
	 */
	void SetGPSFileName(G4String gpsFileName)
	{ GPSFile = gpsFileName; }
	/**
	 *  @return path to the file containing the particle source's settings
	 */
	G4String GetGPSFileName() const
	{ return GPSFile; }

	/**
	 *  Set path to the file which the (shortened) simulation results is to be written to.
	 */
	void SetDataFileName(G4String dataFileName)
	{ DataFile = dataFileName; }
	/**
	 *  @return path to the file which the (shortened) simulation results is to be written to
	 */
	G4String GetDataFileName() const
	{ return DataFile; }

	/**
	 *  Set path to the file which the simulation results is to be written to.
	 */
	void SetControlFileName(G4String controlFileName)
	{ ControlFile = controlFileName; }
	/**
	 *  @return path to the file which the simulation results is to be written to
	 */
	G4String GetControlFileName() const
	{ return ControlFile; }

	/**
	 *  Set whether to use G4Sipm instead of G4PhotonDetector.
	 */
	void SetUseG4Sipm(G4bool useG4Sipm)
	{ UseG4Sipm = useG4Sipm; }
	/**
	 *  @return G4bool, whether to use G4Sipm instead of G4PhotonDetector
	 */
	G4bool GetUseG4Sipm() const
	{ return UseG4Sipm; }

	/**
	 *  Set the SiPM model file path (or empty string for the generic model).
	 */
	void SetSipmModelFile(G4String sipmModelFile)
	{ SipmModelFile = sipmModelFile; }
	/**
	 *  @return path to the SiPM model properties file (empty = generic model)
	 */
	G4String GetSipmModelFile() const
	{ return SipmModelFile; }

	/**
	 *  @return pointer to the HDF5 output writer
	 */
	HDF5Writer* GetHDF5Writer() const
	{ return H5Writer; }

	/**
	 *  Set path to the HDF5 output file.
	 */
	void SetHDF5FileName(G4String hdf5FileName)
	{ HDF5File = hdf5FileName; }
	/**
	 *  @return path to the HDF5 output file
	 */
	G4String GetHDF5FileName() const
	{ return HDF5File; }

	/**
	 *  Set the path to a geometry-manifest file to build the detector from.
	 */
	void SetManifestInputFile(G4String manifestInputFile)
	{ ManifestInputFile = manifestInputFile; }
	/**
	 *  @return path to the input geometry-manifest file
	 */
	G4String GetManifestInputFile() const
	{ return ManifestInputFile; }

	/**
	 *  Set the path that the geometry manifest used for this run is dumped to
	 *  (empty = do not dump).
	 */
	void SetManifestDumpFile(G4String manifestDumpFile)
	{ ManifestDumpFile = manifestDumpFile; }
	/**
	 *  @return path that the geometry manifest is dumped to ("" = no dump)
	 */
	G4String GetManifestDumpFile() const
	{ return ManifestDumpFile; }

};

#endif
