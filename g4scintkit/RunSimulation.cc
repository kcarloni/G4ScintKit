/*
 * author:      Erik Dietz-Laursonn
 * institution: Physics Institute 3A, RWTH Aachen University, Aachen, Germany
 * copyright:   Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License
 */


#include <iostream>
#include <unistd.h>

#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>

#include <G4RunManager.hh>
#include <G4UImanager.hh>
#include <G4UIExecutive.hh>
#include <G4VisExecutive.hh>

#include <G4HadronicProcessStore.hh>
#include <Randomize.hh>
#include <PhysicsList.hh>
#include <DetectorConstruction.hh>
#include <GeneralParticleSource.hh>
#include <PhotonListSource.hh>
#include <ParticleListSource.hh>
#include <GeneralParticleSourceMessenger.hh>
#include <RunAction.hh>
#include <SimulationMessenger.hh>
#include <EventAction.hh>
#include <TrackingAction.hh>
#include <SteppingAction.hh>

// GODDeSS
#include <GODDeSS_Messenger.hh>
#include <ScintillatorTileConstructor.hh>
#include <FibreConstructor.hh>
#include <PhotonDetectorConstructor.hh>

G4String assembleOutputFileName(G4String outDir, G4String outFileName, G4String addPhrase);

void ParseCommandLine(int argc, char** argv);
G4bool getBooleanOption(int arg, char** argv, G4String programParameterString, G4bool & variable, G4String coutString = "");
G4bool getStringlikeOption(int & arg, char** argv, G4String programParameterString, G4String & variable, G4String coutStringFragment);
G4bool getIntegerOption(int & arg, char** argv, G4String programParameterString, G4int & variable, G4String coutStringFragment);
void ParseInitFile(G4String initFile);

G4bool applyChecked(G4UImanager* UI, const G4String& command);
G4bool executeMacroChecked(G4UImanager* UI, const G4String& macroPath);

void Usage();
void Help();

// // index for simulation
// G4int NumSetup;

G4String InitFile;
G4String MacroFile;
G4int NumEvents;
G4bool UsePhotonListSource;
G4bool UseParticleListSource;
G4String PSInputFile;
G4bool SearchOverlaps;
G4String OutDir;
G4String Phrase;
G4int ControlVerbosity;
G4int RunVerbosity;
G4int EventVerbosity;
G4int TrackingVerbosity;
G4int HadronicVerbosity;
G4bool DontTrackOpticalPhotons;

G4int RunID;
G4int Seed;
G4bool Quiet;
G4String ManifestFile;




int main(int argc, char** argv)
{
/// set variables
	InitFile = "";
	MacroFile = "";
	NumEvents = -1;
	UsePhotonListSource = false;
	UseParticleListSource = false;
	PSInputFile = "";
	SearchOverlaps = false;
	OutDir = "";
	G4String dataSubOutDir = "Data/";
	Phrase = "";
	ControlVerbosity = 0;
	RunVerbosity = 0;
	EventVerbosity = 0;
	TrackingVerbosity = 0;
	HadronicVerbosity = 0;
	DontTrackOpticalPhotons = false;
	RunID = -1;
	Seed = -1;
	Quiet = false;
	ManifestFile = "";

	ParseCommandLine(argc, argv);

	if(InitFile != "") ParseInitFile(InitFile);

	// get environment variable "SIMDIR":
	G4String simDir = "";
	if(getenv("SIMDIR")) simDir = getenv("SIMDIR");
	else
	{
		G4cerr << G4endl << "The environment variable \"SIMDIR\" (pointing to the directory of your simulation code) has not been specified!" << G4endl << G4endl;
		return 1;
	}

	if(OutDir == "" && getenv("BUILDDIR")) OutDir = getenv("BUILDDIR");

	std::string::reverse_iterator dirIter = OutDir.rbegin();
	if ((G4String) *dirIter != "/") OutDir += "/";

	// get current directory:
	// 	G4String pwd = getenv("PWD");

	// change current working directory:
	G4String buildDir = "";
	if(getenv("BUILDDIR")) buildDir = getenv("BUILDDIR");
	else
	{
		G4cerr << G4endl << "The environment variable \"BUILDDIR\" (pointing to the directory of where the executable is build) has not been specified!" << G4endl << G4endl;
		return 1;
	}
	// Paths given on the command line are relative to the user's shell, but we
	// are about to chdir into the build directory — so resolve them against the
	// current working directory first. Without this, `--manifest rel/path`
	// silently resolves against BUILDDIR instead and fails with a "cannot read"
	// message that gives no hint why.
	{
		char cwdBuf[4096];
		if(getcwd(cwdBuf, sizeof(cwdBuf)))
		{
			const G4String cwd(cwdBuf);
			G4String* const relocatable[] =
				{ &ManifestFile, &OutDir, &PSInputFile, &MacroFile };
			for(size_t i = 0; i < sizeof(relocatable) / sizeof(relocatable[0]); ++i)
			{
				G4String& path = *relocatable[i];
				if(!path.empty() && path[0] != '/') path = cwd + "/" + path;
			}
		}
	}

	chdir(buildDir.c_str());

	// set minimal and maximal possible photon energy and create a vector containing them:
	G4double energiesMin = 1.0 * CLHEP::eV;   // at least minimal and maximal possible photon energy, necessary to define the refraction index etc. for the full spectrum of possible photon energies
	G4double energiesMax = 7.0 * CLHEP::eV;   // from ~180nm to ~1000nm -> Hamamatsu S13370 (UV-sensitive SiPM)

	// GODDeSS: energy range for the property distributions
	vector<G4double> energyRangeVector;
	G4double energyRangeVectorSize = 10;
	for(int iter = 0; iter < energyRangeVectorSize; iter++) energyRangeVector.push_back( energiesMin + iter * (energiesMax - energiesMin) / (energyRangeVectorSize - 1) );

/// initialise the simulation:
	// GODDeSS Messenger:
	GODDeSS_Messenger * goddessMessenger = new GODDeSS_Messenger(energyRangeVector);

	// create simulation messenger:
	SimulationMessenger * simulationMessenger = new SimulationMessenger(goddessMessenger);
	simulationMessenger->SetUsePhotonListSource(UsePhotonListSource);
	simulationMessenger->SetUseParticleListSource(UseParticleListSource);
	simulationMessenger->SetSearchOverlaps(SearchOverlaps);
	simulationMessenger->SetTrackPhotons(!DontTrackOpticalPhotons);
	simulationMessenger->SetQuiet(Quiet);

	// compose the HDF5 output file name
	G4String h5OutFile = assembleOutputFileName(OutDir + dataSubOutDir, "g4sim.h5", Phrase);
	simulationMessenger->SetHDF5FileName(h5OutFile);

	// geometry manifest input file and the per-run dump path
	simulationMessenger->SetManifestInputFile(ManifestFile);
	simulationMessenger->SetManifestDumpFile(OutDir + dataSubOutDir + "geometry.manifest");

	// Construct the (default) run manager:
	G4RunManager * runManager = new G4RunManager;

	// set mandatory initialization classes and create constructors:
	G4VModularPhysicsList * physicsList = new PhysicsList();

	// GODDeSS Constructor:
	ScintillatorTileConstructor * scintillatorTileConstructor = new ScintillatorTileConstructor(
		physicsList, 
		goddessMessenger->GetPropertyToolsManager(), 
		goddessMessenger->GetDataStorage(), 
		SearchOverlaps
	);
	goddessMessenger->SetScintillatorTileConstructor(scintillatorTileConstructor);

	FibreConstructor * fibreConstructor = new FibreConstructor(
		physicsList, 
		goddessMessenger->GetPropertyToolsManager(), 
		goddessMessenger->GetDataStorage(), 
		SearchOverlaps
	);
	goddessMessenger->SetFibreConstructor(fibreConstructor);

	PhotonDetectorConstructor * photonDetectorConstructor = new PhotonDetectorConstructor(
		physicsList, 
		goddessMessenger->GetPropertyToolsManager(), 
		goddessMessenger->GetDataStorage(), 
		SearchOverlaps
	);
	goddessMessenger->SetPhotonDetectorConstructor(photonDetectorConstructor);

	OpticalCouplingConstructor * opticalCouplingConstructor = new OpticalCouplingConstructor(
		physicsList, 
		goddessMessenger->GetPropertyToolsManager(), 
		goddessMessenger->GetDataStorage(), 
		SearchOverlaps);
	goddessMessenger->SetOpticalCouplingConstructor(opticalCouplingConstructor);

	// initialise the physics list (this has to be done AFTER the physics processes have been defined!!!)
	runManager->SetUserInitialization(physicsList);

	// this has to be done AFTER the simulationMessenger has been filled!!!
	DetectorConstruction * detector = new DetectorConstruction(simulationMessenger);   
	runManager->SetUserInitialization(detector);

	// set mandatory user action classes:
	GeneralParticleSourceMessenger * gpsm = 0;
	ParticleListSource* particleListSource = 0;
	if(UseParticleListSource)
	{
		particleListSource = new ParticleListSource();
		particleListSource->SetEventIdCallback([simulationMessenger](int csvEventId) {
			simulationMessenger->SetCurrentCsvEventId(csvEventId);
		});
		runManager->SetUserAction(particleListSource);
	}
	else if(UsePhotonListSource)
	{
		PhotonListSource* pls = new PhotonListSource();
		runManager->SetUserAction(pls);
	}
	else
	{
		GeneralParticleSource* gps = new GeneralParticleSource();
		gpsm = gps->getMessenger();
		runManager->SetUserAction(gps);
	}

	// A Run is started by /run/beamOn and may consist of one or more events.
// 	G4UserRunAction* run_action = new RunAction();
	RunAction* run_action = new RunAction(simulationMessenger);
	runManager->SetUserAction(run_action);

	// An Event covers everything happening when the particle gun is fired once (no matter how many particles it fires).
	EventAction* event_action = new EventAction(simulationMessenger);
	runManager->SetUserAction(event_action);

// 	G4UserStackingAction* stacking_action = new ScintiStackingAction(&data);
// 	runManager->SetUserAction(stacking_action);

	// A Track is the full trajectory of a particle, it can be divided into steps (which is the part of the trajectory between two interactions).
	// In Geant4, each particle is described by one G4Track, but the processing of a particle's G4Track is suspended every time the original particle creates new particles (G4Tracks). Thus, for the user (i.e. in the G4UserTrackingAction and the G4UserSteppingAction) some particles appear to own several G4Tracks (all with the same TrackID and ParentID), which can lead to problems, e.g. when trying to determine the energy deposit of a single particle (http://hypernews.slac.stanford.edu/HyperNews/geant4/get/eventtrackmanage/1072.html)!!!
	TrackingAction* tracking_action = new TrackingAction(simulationMessenger);
	runManager->SetUserAction(tracking_action);

	// A Step is a part of the full trajectory of a particle, confined by two interactions. The sum of all steps of one particle gives its full trajectory.
	SteppingAction* stepping_action = new SteppingAction(simulationMessenger);
	runManager->SetUserAction(stepping_action);

	// get the pointer to the UI manager to apply commands
	G4UImanager * UI = G4UImanager::GetUIpointer();

	// Verbosity (applied before Initialize; hadronic verbose must wait until after)
	if(!applyChecked(UI, "/control/verbose " + boost::lexical_cast<G4String>(ControlVerbosity))) return 1;
	if(!applyChecked(UI, "/run/verbose " + boost::lexical_cast<G4String>(RunVerbosity))) return 1;
	if(!applyChecked(UI, "/event/verbose " + boost::lexical_cast<G4String>(EventVerbosity))) return 1;
	if(!applyChecked(UI, "/tracking/verbose " + boost::lexical_cast<G4String>(TrackingVerbosity))) return 1;

/// initialise G4 kernel (i.e. construct detector, define physics,...)
	runManager->Initialize();

	// Suppress hadronic process summary via direct C++ API.
	// The UI command /process/had/verbose is unreliable because
	// G4RunManagerKernel::RunInitialization() overrides it from the
	// kernel's verbose level. Setting both ensures suppression.
	G4HadronicProcessStore::Instance()->SetVerbose(HadronicVerbosity);
	runManager->SetVerboseLevel(0);

	// Random seed must be set AFTER Initialize() — initialization can
	// reset the random engine state, discarding any earlier seed.
	// Use CLHEP API directly (UI command /random/setSeed is unreliable).
	if(Seed >= 0)
	{
		G4Random::setTheSeed(static_cast<long>(Seed));
		G4cout << "Random seed set to: " << Seed
		       << " (engine: " << G4Random::getTheSeed() << ")" << G4endl;
	}

	// load the particle source input file
	if(UseParticleListSource)
	{
		if(!applyChecked(UI, "/particleList/path " + PSInputFile)) return 1;
	}
	else if(UsePhotonListSource)
	{
		if(!applyChecked(UI, "/pls/photonListPath " + PSInputFile)) return 1;
	}
	else
	{
		if(!executeMacroChecked(UI, PSInputFile)) return 1;

		G4String gpsOutFile = gpsm->getOutputFileName();
		gpsOutFile = assembleOutputFileName(OutDir + dataSubOutDir, gpsOutFile, Phrase);
		gpsm->setOutputFileName(gpsOutFile);
		simulationMessenger->SetGPSFileName(gpsOutFile);
	}

	// get the settings from a macro
	if(MacroFile != "")
	{
		G4cout << "Using macro file : " << MacroFile << G4endl;
		if(!executeMacroChecked(UI, MacroFile)) return 1;
	}

/// run Geant4
	// batch mode: no visualisation
	if(UseParticleListSource && particleListSource && particleListSource->GetNumberOfRuns() > 0 && NumEvents > 0)
	{
		// Loop over runs from the particle list
		for(size_t r = 0; r < particleListSource->GetNumberOfRuns(); r++)
		{
			int rid = particleListSource->GetRunId(r);
			size_t nev = particleListSource->GetNumEventsInRun(r);
			simulationMessenger->SetCurrentRunId(rid);
			G4cout << "executing run " << rid << " with " << nev << " events" << G4endl;
			if(!applyChecked(UI, "/run/beamOn " + boost::lexical_cast<G4String>(nev))) return 1;
			particleListSource->FreeRunData(r);
			if(r + 1 < particleListSource->GetNumberOfRuns())
				particleListSource->AdvanceRun();
		}
	}
	else if(NumEvents > 0)
	{
		// set run ID if specified
		if(RunID >= 0)
		{
			// Geant4 has no /run/setRunIDCounter UI command — this was previously
			// issued as one, which G4UImanager rejected as "command not found"
			// (rc 100) while nothing checked the code, so --runID silently did
			// nothing. SetRunIDCounter is a G4RunManager method; the value flows
			// into the HDF5 group name via aRun->GetRunID() in RunAction.
			runManager->SetRunIDCounter(RunID);
		}
		G4cout << "executing command: run/beamOn " << NumEvents << G4endl;
		if(!applyChecked(UI, "/run/beamOn " + boost::lexical_cast<G4String>(NumEvents))) return 1;
	}
	// interactive mode: define visualization and UI terminal
	else if(NumEvents < 0)
	{
		/// Visualisation:
		// initialise visualisation manager
		G4VisManager * visManager = new G4VisExecutive("quiet");
		visManager->Initialize();

		// create session
		G4UIExecutive * session = new G4UIExecutive(argc, argv);

		// get visualisation commands
		// Not fatal, unlike the macros above: a missing visualisation driver can
		// make a vis.mac command fail without affecting the geometry or physics,
		// and dropping into the session is still more useful than refusing to.
		if(!executeMacroChecked(UI, simDir + "/vis.mac"))
			G4cerr << "RunSimulation: continuing without full visualisation setup."
			       << G4endl;

		// start session
		session->SessionStart();

		/// Job termination:
		// Everything initialised by the G4RunManager initialisation is owned and deleted by the G4RunManager and should not be deleted manually in the main() program!
		delete session;
		delete visManager;
	}

/// Job termination:
	// Everything initialised by the G4RunManager initialisation is owned and deleted by the G4RunManager and should not be deleted manually in the main() program!
	delete runManager;

	delete simulationMessenger;

	// GODDeSS
	delete goddessMessenger;
	delete scintillatorTileConstructor;
	delete fibreConstructor;
	delete photonDetectorConstructor;

// 	system( ("cd " + pwd).c_str() );
	return 0;
}



G4String assembleOutputFileName(G4String outDir, G4String outFileName, G4String addPhrase)
{
	system( ("mkdir -p " + outDir).c_str() );

	boost::match_results<std::string::const_iterator> result;
	if (regex_search(outFileName, result, boost::regex("([^.]+)[.]([^.]+)"), boost::match_all)) outFileName = result[1] + addPhrase + "." + result[2];
	else  outFileName += addPhrase;

	outFileName = outDir + outFileName;

	return outFileName;
}



void ParseCommandLine(int argc, char** argv)
{
	if(argc!=1)
	{
		for(int arg = 1; arg < argc; arg++)   //check if the help-option has been chosen
		{
			if( (strcmp(argv[arg], "--help") == 0) || (strcmp(argv[arg], "-h") == 0) )
			{
				Help();
				exit(-1);
			}
		}

		for(int arg = 1; arg < argc; arg++)
		{
			// init from file
			if(strcmp(argv[arg], "--init") == 0)
			{
				if(argc == 3)
				{
					if(argv[arg+1] && argv[arg+1][0] != '-')
					{
						InitFile = argv[arg+1];
						G4cout << "Used init file: " << InitFile << G4endl;
						break;
					}
					else
					{
						G4cout << "No init file specified!" << G4endl;
						Usage();
						exit(-1);
					}
				}
				else
				{
					G4cout << "Init file AND other options specified!" << G4endl;
					Usage();
					exit(-1);
				}
			}

			if(getStringlikeOption(arg, argv, "--macro", MacroFile, "macro filename")) continue;
			
			// batch mode
			if(getIntegerOption(arg, argv, "--batch", NumEvents, "number of events"))
			{
				G4cout << "Running in batch mode!" << G4endl;
				continue;
			}
			if(getBooleanOption(arg, argv, "--usePhotonList", UsePhotonListSource)) continue;
			if(getBooleanOption(arg, argv, "--useParticleList", UseParticleListSource)) continue;
			if(getStringlikeOption(arg, argv, "--particleSourceInput", PSInputFile, "particle source input file")) continue;
			if(getBooleanOption(arg, argv, "--overlap", SearchOverlaps, "Searching for overlaps in the setup.")) continue;
			if(getStringlikeOption(arg, argv, "--outDir", OutDir, "output directory")) continue;
			if(getStringlikeOption(arg, argv, "--add", Phrase, "phrase to be added to filename")) continue;
			if(getIntegerOption(arg, argv, "--controlVerbose", ControlVerbosity, "control verbosity")) continue;
			if(getIntegerOption(arg, argv, "--runVerbose", RunVerbosity, "run verbosity")) continue;
			if(getIntegerOption(arg, argv, "--eventVerbose", EventVerbosity, "event verbosity")) continue;
			if(getIntegerOption(arg, argv, "--trackingVerbose", TrackingVerbosity, "tracking verbosity")) continue;
			if(getIntegerOption(arg, argv, "--hadronicVerbose", HadronicVerbosity, "hadronic verbosity")) continue;
			if(getBooleanOption(arg, argv, "--noOpticalPhotonTracking", DontTrackOpticalPhotons, "Don't track optical photons."))
			{
				G4cout << "--------DontTrackOpticalPhotons " << DontTrackOpticalPhotons << G4endl; continue;
			}
			if(getStringlikeOption(arg, argv, "--manifest", ManifestFile, "geometry manifest file")) continue;
			if(getIntegerOption(arg, argv, "--runID", RunID, "run ID")) continue;
			if(getIntegerOption(arg, argv, "--seed", Seed, "random seed")) continue;
			if(getBooleanOption(arg, argv, "--quiet", Quiet, "Suppressing per-event console output.")) continue;
			G4cout << "Unknown option: " << argv[arg] << G4endl;
			Usage();
			exit(-1);
		}
	}
}



G4bool getBooleanOption(int arg, char** argv, G4String programParameterString, G4bool & variable, G4String coutString)
{
	if(strcmp(argv[arg], programParameterString.c_str()) == 0)
	{
		variable = true;

		if(coutString != "") G4cout << G4endl << G4endl << coutString << G4endl << G4endl << G4endl;

		return true;
	}

	return false;
}



G4bool getStringlikeOption(int & arg, char** argv, G4String programParameterString, G4String & variable, G4String coutStringFragment)
{
	if(strcmp(argv[arg], programParameterString.c_str()) == 0)
	{
		if(argv[arg+1] && (argv[arg+1])[0] != '-')
		{
			variable = argv[arg+1];
			arg++;

			return true;
		}

		G4cout << "No " << coutStringFragment << " specified!" << G4endl;
		Usage();
		exit(-1);
	}

	return false;
}



G4bool getIntegerOption(int & arg, char** argv, G4String programParameterString, G4int & variable, G4String coutStringFragment)
{
	if(strcmp(argv[arg], programParameterString.c_str()) == 0)
	{
		if(argv[arg+1] && (argv[arg+1])[0] != '-')
		{
			variable = atoi(argv[arg+1]);
			arg++;

			return true;
		}

		G4cout << "No " << coutStringFragment << " specified!" << G4endl;
		Usage();
		exit(-1);
	}

	return false;
}


// G4bool getDoubleOption(int & arg, char** argv,
//                        G4String programParameterString,
//                        G4double & variable,
//                        G4String coutStringFragment)
// {
//     if (strcmp(argv[arg], programParameterString.c_str()) == 0)
//     {
//         if (argv[arg+1] && (argv[arg+1])[0] != '-')
//         {
//             variable = atof(argv[arg+1]);
//             arg++;

//             return true;
//         }

//         G4cout << "No " << coutStringFragment << " specified!" << G4endl;
//         Usage();
//         exit(-1);
//     }

//     return false;
// }


void ParseInitFile(G4String initFile)
{
	// Open the file.
	FILE * infile;
	infile = fopen(initFile.c_str(), "r");

	if(!infile)
	{
		std::cerr << "Cannot open file \"" << initFile << "\"" << std::endl;
		exit(-1);
	}
	else
	{
		std::vector<G4String> optionVector;
		optionVector.push_back("ProgramNameDummy");   // the first entry of argv (argv[0]) is the name of the program

		while(!feof(infile))
		{
			char line[1000];

			// get the lines
			fgets(line, 1000, infile);

			if(strncmp(line, "#", strlen("#")) != 0)   // if the line is not commented out
			{
				char rchar1[200];
				char rchar2[200];
				int numVariables = sscanf(line, "%s %s", rchar1, rchar2);
				if (numVariables > 0) optionVector.push_back(rchar1);
				if (numVariables > 1) optionVector.push_back(rchar2);
			}
		}

		int ARGC = optionVector.size();
		char **ARGV = (char**)malloc(ARGC * sizeof *ARGV);

		for(int iter = 0; iter < ARGC; iter++) {
			ARGV[iter] = (char*)malloc(strlen(optionVector[iter]) + 1);
			strcpy(ARGV[iter], optionVector[iter]);
		}

		ParseCommandLine(ARGC, ARGV);
	}

	fclose(infile);
}



void Usage()
{
	G4cout << "Type \"./RunSimulation --help\" for run options!" << G4endl;
}



void Help()
{
	G4cout << G4endl;
	G4cout << "== RunSimulation ========================================================================================================================" << G4endl << G4endl;
	G4cout << "   Options:"                                                                                                                               << G4endl;
	G4cout << "            --help                          : Show this output."                                                                           << G4endl;
	G4cout << "            --init <init file>              : Get run options from <init file>."                                                           << G4endl;
	G4cout << "                                              In this case, no other options (but \"--help\") when calling the program."                   << G4endl;
	G4cout << "            --macro <macroname.mac>         : Run with the settings specified in this macro."                                              << G4endl;
	G4cout << "            --batch <num of events>         : Force program to run in batch mode (without visualisation, i.e. faster)."                    << G4endl;
	G4cout << "                                              The given number of events will be simulated."                                               << G4endl;
	G4cout << "            --usePhotonList                 : Use a list with photons to define the primary particles."                                    << G4endl;
	G4cout << "                                              This list has to be specified with the \"--particleSourceInput\" option."                    << G4endl;
	G4cout << "            --useParticleList              : Use a CSV file with arbitrary particles to define the primary particles."                     << G4endl;
	G4cout << "                                              CSV columns: event_id,pdg,t_ns,x_mm,y_mm,z_mm,ekin_MeV,dx,dy,dz"                           << G4endl;
	G4cout << "                                              This file has to be specified with the \"--particleSourceInput\" option."                    << G4endl;
	G4cout << "            --particleSourceInput <filename>: Use <filename> as input for the particle source."                                            << G4endl;
	G4cout << "                                              This can either be a macro for the General Particle Source (if the \"--usePhotonList\""      << G4endl;
	G4cout << "                                              option IS NOT set) or a list with photons (if the \"--usePhotonList\" option IS set)."       << G4endl;
	G4cout << "            --overlap                       : Search for overlaps in the setup. Default is: \"false\""                                     << G4endl;
	G4cout << "            --fibre <filename.properties>   : Get material properties of the fibre from <filename.properties>"                             << G4endl;
	G4cout << "            --outDir <directory>            : Define output directory. This directory must exisit! Default is: \"./\""                     << G4endl;
	G4cout << "            --add <phrase>                  : Add <phrase> to HDF5 output filename (simulation.h5)."                                       << G4endl;
	G4cout << "            --controlVerbose <number>       : Specify the GEANT4 verbosity level set using \"/control/verbose\". Default is: \"0\""        << G4endl;
	G4cout << "            --runVerbose <number>           : Specify the GEANT4 verbosity level set using \"/run/verbose\". Default is: \"0\""            << G4endl;
	G4cout << "            --eventVerbose <number>         : Specify the GEANT4 verbosity level set using \"/event/verbose\". Default is: \"0\""          << G4endl;
	G4cout << "            --trackingVerbose <number>      : Specify the GEANT4 verbosity level set using \"/tracking/verbose\". Default is: \"0\""       << G4endl;
	G4cout << "            --hadronicVerbose <number>     : Specify the GEANT4 verbosity level set using \"/process/had/verbose\". Default is: \"0\""  << G4endl;
	G4cout << "            --noOpticalPhotonTracking       : Do not track optical photons when simulating events."                                        << G4endl;
	G4cout << "            --manifest <filename>           : REQUIRED. Geometry manifest describing the setup to build."                                  << G4endl;
	G4cout << "                                              A flat text file of SETUP/SCINT/FIBER/WRAP/SIPM/CASING lines; file order is"                 << G4endl;
	G4cout << "                                              placement order. See docs/manifest_format.md. Material paths inside it may be"              << G4endl;
	G4cout << "                                              relative to $GODDESS, which is how a manifest stays portable between machines."              << G4endl;
	G4cout << "            --runID <number>                : Run ID stamped into the HDF5 output. Default is: \"0\""                                      << G4endl;
	G4cout << "            --seed <number>                 : Random-number seed, for reproducible runs."                                                  << G4endl;
	G4cout << "            --quiet <true|false>            : Suppress per-event console output. Default is: \"false\""                                    << G4endl << G4endl;
	G4cout << "=========================================================================================================================================" << G4endl << G4endl;
}


/**
 *  Apply a UI command, treating a rejection as fatal.
 *
 *  G4UImanager reports an illegal command but has no way to refuse to continue;
 *  nothing above it checks the return code either. A rejected command means the
 *  run is not configured the way the caller asked for, so continuing produces
 *  physics that silently disagrees with the recorded arguments. Returns false
 *  when the caller should abort.
 */
G4bool applyChecked(G4UImanager* UI, const G4String& command)
{
	G4int rc = UI->ApplyCommand(command);
	if(rc == 0) return true;

	G4cerr << G4endl
	       << "RunSimulation: Geant4 rejected the command <" << command << ">"
	       << G4endl
	       << "  (G4UImanager return code " << rc << "). Refusing to run: the"
	       << G4endl
	       << "  simulation would not be configured as requested." << G4endl << G4endl;
	return false;
}

/**
 *  Execute a macro file, treating any failure inside it as fatal.
 *
 *  Two checks are needed here, and neither subsumes the other.
 *
 *  GetLastReturnCode() is the load-bearing one. `/control/execute` succeeds as a
 *  command even when a command *inside* the macro is rejected: G4UIbatch stops
 *  reading the file at that point ("Batch is interrupted!!") and records the
 *  code in G4UImanager, which must be read back explicitly. Without it a single
 *  bad line silently drops that line and every line after it, leaving the rest
 *  of the macro's settings at their defaults. It also catches a macro that
 *  cannot be opened at all, since G4UIbatch's constructor records
 *  fParameterUnreadable in that case.
 *
 *  The applyChecked() call below looks redundant next to that, but is not.
 *  G4UImanager resets lastRC only inside ExecuteMacroFile, so a rejection at the
 *  *command* level -- an empty or unparseable path, which never reaches
 *  ExecuteMacroFile -- leaves lastRC holding its previous value. That value is
 *  zero here, since we abort on any nonzero code, so without this check such a
 *  failure would read back as success.
 *
 *  Returns false when the caller should abort.
 */
G4bool executeMacroChecked(G4UImanager* UI, const G4String& macroPath)
{
	if(!applyChecked(UI, "/control/execute " + macroPath)) return false;

	G4int rc = UI->GetLastReturnCode();
	if(rc == 0) return true;

	G4cerr << G4endl
	       << "RunSimulation: a command in the macro <" << macroPath << "> was"
	       << G4endl
	       << "  rejected (return code " << rc << "), so Geant4 stopped reading it"
	       << G4endl
	       << "  there. That line and every line after it were not applied."
	       << G4endl
	       << "  See the \"Illegal parameter\" message above for the offending"
	       << G4endl
	       << "  command. Refusing to run with a partially-applied macro."
	       << G4endl << G4endl;
	return false;
}
