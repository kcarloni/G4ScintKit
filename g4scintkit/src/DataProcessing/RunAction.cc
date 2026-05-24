/*
 * author:      Erik Dietz-Laursonn
 * institution: Physics Institute 3A, RWTH Aachen University, Aachen, Germany
 * copyright:   Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License
 */


#include <G4EventManager.hh>

#include <RunAction.hh>



// class variables begin with capital letters, local variables with small letters



/**
 *  Function that will be called by Geant4 at the beginning of each run.
 */
void RunAction::BeginOfRunAction(const G4Run* aRun)
{
	RunInformation->ResetDefaults();
	RunInformation->SetNumberOfEvents(aRun->GetNumberOfEventToBeProcessed());

	// Use the CSV run_id if set, otherwise fall back to Geant4's internal counter
	G4int runID = Messenger->GetCurrentRunId() >= 0
	            ? Messenger->GetCurrentRunId()
	            : aRun->GetRunID();
	if(!Messenger->GetQuiet())
		G4cout << "### Run " << runID << " start." << G4endl;

	// Open HDF5 writer for this run
	Messenger->GetHDF5Writer()->open(Messenger->GetHDF5FileName(), runID);

	Timer->Start();
}

/**
 *  Function that will be called by Geant4 at the end of each run.
 */
void RunAction::EndOfRunAction(const G4Run* aRun)
{
	// Write accumulated data to HDF5 and close
	Messenger->GetHDF5Writer()->close();

	G4int numberOfEventsProcessed = aRun->GetNumberOfEvent();

	Timer->Stop();
	if(!Messenger->GetQuiet())
	{
		G4cout << G4endl << "number of events processed = " << numberOfEventsProcessed << G4endl;
		G4cout << "required: " << Timer->GetRealElapsed() << "s" << G4endl << G4endl;
	}
}
