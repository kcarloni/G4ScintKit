/*
 * author:      Erik Dietz-Laursonn
 * institution: Physics Institute 3A, RWTH Aachen University, Aachen, Germany
 * copyright:   Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License
 */


#include <G4EventManager.hh>
#include <G4OpticalPhoton.hh>

#include <UserRunInformation.hh>

#include <EventAction.hh>

#include <algorithm>
#include <vector>
#include <map>
#include <cmath>



// class variables begin with capital letters, local variables with small letters



/**
 *  Function that will be called by Geant4 at the beginning of each event.
 */
void EventAction::BeginOfEventAction(const G4Event* theEvent)
{
	// Use CSV event_id if available, otherwise Geant4's sequential counter
	EventID = Messenger->GetCurrentCsvEventId() >= 0
	        ? Messenger->GetCurrentCsvEventId()
	        : theEvent->GetEventID();
	if(!Messenger->GetQuiet())
	{
		G4cout << G4endl << "-----------------------------------------------------------------------------" << G4endl;
		G4cout << "EventID = " << EventID << G4endl << G4endl;
	}

	G4EventManager::GetEventManager()->SetUserInformation(new UserEventInformation);

	EventInformation = (UserEventInformation*)theEvent->GetUserInformation();
	EventInformation->clean();
	EventInformation->SetNumberOfPrimaryParticles(theEvent->GetNumberOfPrimaryVertex());

	GoddessDataStorage->clean();
}

/**
 *  Function that will be called by Geant4 at the end of each event.
 */
void EventAction::EndOfEventAction(const G4Event* theEvent)
{
	EventInformation = (UserEventInformation*)theEvent->GetUserInformation();

	processEventData(theEvent);

	printEventSummary();
}



void EventAction::processEventData(const G4Event* theEvent)
{
	auto* h5Writer = Messenger->GetHDF5Writer();

	// --- Variables for primary particle (used for console output + RunInformation) ---
	G4String name_primary = "";
	G4int ID_primary = 0;
	G4ThreeVector initialPosition_primary;
	G4ThreeVector initialMomentum_primary;
	G4ThreeVector scintiHitPoint_primary;
	G4ThreeVector scintiHitMomentum_primary;
	G4double globalHitTime = NAN;
	G4double deltaE_primary = NAN;
	G4double scintiPathLength_primary = 0;
	G4double totalSecondaryEnergy = 0.0;

	// Accumulated primary energy deposition and earliest hit time (for particle_hits table)
	G4double totalPrimaryEnergy = 0.0;
	G4double firstHitTime = NAN;

	// --- Optical photon counts ---
	G4int CerenkovPhotonsFromPrimary = 0;
	G4int scintiPhotonsFromPrimary = 0;
	G4int WLSPhotonsFromPrimary = 0;
	G4int CerenkovPhotonsFromSecondary = 0;
	G4int scintiPhotonsFromSecondary = 0;
	G4int WLSPhotonsFromSecondary = 0;

	// --- For console output ---
	std::map<G4String, G4int> names_secondary;
	std::map<G4String, G4int> names_processes;

	// --- SiPM hit times for quantile computation ---
	std::vector<double> sipmHitTimes;

	G4int numParticles = EventInformation->GetNumberOfParticles();
	for (int iter = 1; iter < numParticles + 1; iter++)
	{
		G4String particleName = EventInformation->GetParticleName(iter);
		G4bool isPrimary = EventInformation->GetParticleIsPrimary(iter);

		if (isPrimary)
		{
			name_primary = particleName;

			G4int particleID = EventInformation->GetParticleID(iter);
			if (!std::isnan(particleID))
			{
				RunInformation->SetMinMaxValue_Control_PrimaryParticleID(particleID);
				ID_primary = particleID;
			}

			initialPosition_primary = EventInformation->GetInitialPosition(iter);
			RunInformation->SetMinMaxValue_Control_PrimaryParticleInitialPositionX_mm(initialPosition_primary.x() / CLHEP::mm);
			RunInformation->SetMinMaxValue_Control_PrimaryParticleInitialPositionY_mm(initialPosition_primary.y() / CLHEP::mm);
			RunInformation->SetMinMaxValue_Control_PrimaryParticleInitialPositionZ_mm(initialPosition_primary.z() / CLHEP::mm);

			initialMomentum_primary = EventInformation->GetInitialMomentum(iter);

			G4double particleMass = RunInformation->GetParticleMass(particleName);
			G4double initialEnergy = sqrt(pow(particleMass, 2) + pow(initialMomentum_primary.mag(), 2)) - particleMass;
			RunInformation->SetMinMaxValue_Control_PrimaryParticleInitialEnergy_MeV(initialEnergy / CLHEP::MeV);
			if (particleMass)
			{
				G4double initialBetaGamma = initialMomentum_primary.mag() / particleMass;
				RunInformation->SetMinMaxValue_Control_PrimaryParticleInitialBetaGamma(initialBetaGamma);
			}

			globalHitTime = GoddessDataStorage->GetScintillatorHitTime(iter);
			if (!std::isnan(globalHitTime))
				RunInformation->SetMinMaxValue_Control_GlobalScintiHitTime_ns(globalHitTime / CLHEP::ns);

			scintiHitPoint_primary = GoddessDataStorage->GetScintillatorHitPoint(iter);
			RunInformation->SetMinMaxValue_ScintiHitPointX_mm(scintiHitPoint_primary.x() / CLHEP::mm);
			RunInformation->SetMinMaxValue_ScintiHitPointY_mm(scintiHitPoint_primary.y() / CLHEP::mm);
			RunInformation->SetMinMaxValue_ScintiHitPointZ_mm(scintiHitPoint_primary.z() / CLHEP::mm);
			RunInformation->SetMinMaxValue_Control_ScintiHitPointX_mm(scintiHitPoint_primary.x() / CLHEP::mm);
			RunInformation->SetMinMaxValue_Control_ScintiHitPointY_mm(scintiHitPoint_primary.y() / CLHEP::mm);
			RunInformation->SetMinMaxValue_Control_ScintiHitPointZ_mm(scintiHitPoint_primary.z() / CLHEP::mm);

			scintiHitMomentum_primary = GoddessDataStorage->GetScintillatorHitMomentum(iter);

			if (particleName != G4OpticalPhoton::OpticalPhotonDefinition()->GetParticleName())
			{
				deltaE_primary = GoddessDataStorage->GetEnergyDepositionInScintillator(iter);
				RunInformation->SetMinMaxValue_Control_PrimaryParticleEnergyDepositionInScintillator_MeV(deltaE_primary / CLHEP::MeV);
			}

			scintiPathLength_primary = GoddessDataStorage->GetPathLengthInScintillator(iter);
			RunInformation->SetMinMaxValue_Control_PrimaryParticlePathLengthInScintillator_MeV(scintiPathLength_primary / CLHEP::mm);

			// Write input table row (1 per primary)
			G4ThreeVector initDir = initialMomentum_primary.mag() > 0.0
				? initialMomentum_primary.unit() : G4ThreeVector(0, 0, 0);
			h5Writer->addInputParticle(
				EventID,
				std::string(name_primary.c_str()),
				ID_primary,
				initialPosition_primary.x() / CLHEP::mm,
				initialPosition_primary.y() / CLHEP::mm,
				initialPosition_primary.z() / CLHEP::mm,
				initialEnergy / CLHEP::MeV,
				initDir.x(),
				initDir.y(),
				initDir.z());

			// Accumulate for particle_hits table
			if (!std::isnan(deltaE_primary))
				totalPrimaryEnergy += deltaE_primary;
			if (!std::isnan(globalHitTime))
			{
				if (std::isnan(firstHitTime) || globalHitTime < firstHitTime)
					firstHitTime = globalHitTime;
			}
		}
		else
		{
			// --- Secondary particle ---
			try {
				G4int number = names_secondary.at(particleName);
				number++;
				names_secondary[particleName] = number;
			} catch (...) {
				names_secondary[particleName] = 1;
			}

			G4int particleID = EventInformation->GetParticleID(iter);
			if (!std::isnan(particleID))
				RunInformation->SetMinMaxValue_Control_SecondaryParticleID(particleID);

			G4bool isParentPrimary = EventInformation->GetParentIsPrimary(iter);
			G4String productionMechanism = EventInformation->GetProductionMechanism(iter);

			try {
				G4int number = names_processes.at(productionMechanism);
				number++;
				names_processes[productionMechanism] = number;
			} catch (...) {
				names_processes[productionMechanism] = 1;
			}

			if (particleName == G4OpticalPhoton::OpticalPhotonDefinition()->GetParticleName())
			{
				// Optical photon counting
				if (isParentPrimary)
				{
					if (productionMechanism == "Cerenkov") CerenkovPhotonsFromPrimary++;
					if (productionMechanism == "Scintillation") scintiPhotonsFromPrimary++;
					if (productionMechanism == "OpWLS") WLSPhotonsFromPrimary++;
				}
				else
				{
					if (productionMechanism == "Cerenkov") CerenkovPhotonsFromSecondary++;
					if (productionMechanism == "Scintillation") scintiPhotonsFromSecondary++;
					if (productionMechanism == "OpWLS") WLSPhotonsFromSecondary++;
				}

				// Timing and energy tracking
				G4ThreeVector initialMomentum = EventInformation->GetInitialMomentum(iter);
				G4double particleMass = RunInformation->GetParticleMass(particleName);
				G4double initialEnergy = sqrt(pow(particleMass, 2) + pow(initialMomentum.mag(), 2)) - particleMass;
				RunInformation->SetMinMaxValue_Control_OpticalPhotonInitialEnergy_MeV(initialEnergy / CLHEP::MeV);

				G4double globalCreationTime = EventInformation->GetGlobalCreationTime(iter);
				G4double globalAbsorptionTime = EventInformation->GetGlobalAbsorptionTime(iter);

				RunInformation->SetMinMaxValue_Control_DeltaTimeHitCreation_ns(globalCreationTime / CLHEP::ns - globalHitTime / CLHEP::ns);
				RunInformation->SetMinMaxValue_Control_DeltaTimeCreationAbsorption_ns(globalAbsorptionTime / CLHEP::ns - globalCreationTime / CLHEP::ns);
				RunInformation->SetMinMaxValue_Control_DeltaTimeHitAbsorption_ns(globalAbsorptionTime / CLHEP::ns - globalHitTime / CLHEP::ns);

				if (isParentPrimary)
				{
					RunInformation->SetMinMaxValue_Control_DeltaTimeHitCreation_parentPrimary_ns(globalCreationTime / CLHEP::ns - globalHitTime / CLHEP::ns);
					RunInformation->SetMinMaxValue_Control_DeltaTimeCreationAbsorption_parentPrimary_ns(globalAbsorptionTime / CLHEP::ns - globalCreationTime / CLHEP::ns);
					RunInformation->SetMinMaxValue_Control_DeltaTimeHitAbsorption_parentPrimary_ns(globalAbsorptionTime / CLHEP::ns - globalHitTime / CLHEP::ns);
				}
				else
				{
					RunInformation->SetMinMaxValue_Control_DeltaTimeHitCreation_parentSecondary_ns(globalCreationTime / CLHEP::ns - globalHitTime / CLHEP::ns);
					RunInformation->SetMinMaxValue_Control_DeltaTimeCreationAbsorption_parentSecondary_ns(globalAbsorptionTime / CLHEP::ns - globalCreationTime / CLHEP::ns);
					RunInformation->SetMinMaxValue_Control_DeltaTimeHitAbsorption_parentSecondary_ns(globalAbsorptionTime / CLHEP::ns - globalHitTime / CLHEP::ns);
				}

				// Collect SiPM hit times for quantile computation
				if (!std::isnan(globalAbsorptionTime) && GoddessDataStorage->PhotonDetectorWasHit(iter))
				{
					sipmHitTimes.push_back(globalAbsorptionTime / CLHEP::ns);
				}
			}
			else
			{
				// Non-optical secondary particle
				G4double deltaE = GoddessDataStorage->GetEnergyDepositionInScintillator(iter);
				if (!std::isnan(deltaE))
				{
					totalSecondaryEnergy += deltaE;
				}

				RunInformation->SetMinMaxValue_Control_SecondaryParticleEnergyDepositionInScintillator_MeV(deltaE / CLHEP::MeV);

				G4ThreeVector initialMomentum = EventInformation->GetInitialMomentum(iter);
				G4double particleMass = RunInformation->GetParticleMass(particleName);
				G4double initialEnergy = sqrt(pow(particleMass, 2) + pow(initialMomentum.mag(), 2)) - particleMass;
				RunInformation->SetMinMaxValue_Control_SecondaryParticleInitialEnergy_MeV(initialEnergy / CLHEP::MeV);

				if (particleMass)
				{
					G4double initialBetaGamma = initialMomentum.mag() / particleMass;
					RunInformation->SetMinMaxValue_Control_SecondaryParticleInitialBetaGamma(initialBetaGamma);
				}

				G4double globalCreationTime = EventInformation->GetGlobalCreationTime(iter);
				RunInformation->SetMinMaxValue_Control_DeltaTimeHitCreation_secondary_ns(globalCreationTime / CLHEP::ns - globalHitTime / CLHEP::ns);
			}
		}
	}

	// --- Write HDF5 data ---

	// particle_hits: 1 row per event
	G4double edepPri = totalPrimaryEnergy / CLHEP::MeV;
	G4double edepSec = totalSecondaryEnergy / CLHEP::MeV;
	h5Writer->addParticleHits(
		EventID,
		edepPri + edepSec,
		edepPri,
		edepSec,
		firstHitTime / CLHEP::ns);

	// Optical photon summary
	G4int num_Absorbed_inFibre = EventInformation->GetAbsorbedInFibreCount();
	G4int num_Absorbed_inPhotonDetector = EventInformation->GetAbsorbedInPhotonDetectorCount();
	G4int num_Absorbed_inScinti = EventInformation->GetAbsorbedInScintiCount();

	h5Writer->addOpticalPhotonSummary(
		EventID,
		CerenkovPhotonsFromPrimary,
		scintiPhotonsFromPrimary,
		WLSPhotonsFromPrimary,
		CerenkovPhotonsFromSecondary,
		scintiPhotonsFromSecondary,
		WLSPhotonsFromSecondary,
		num_Absorbed_inFibre,
		num_Absorbed_inPhotonDetector,
		num_Absorbed_inScinti
	);

#ifdef USE_G4SIPM
	// --- G4SiPM hit, digi, and voltage trace data ---
	// Probe the hit collections unconditionally — the dynamic_cast filters
	// out non-g4sipm collections, so this is a no-op when no g4sipm housing
	// was placed during DetectorConstruction.
	G4SipmPhotonHitCount = 0;
	{
		G4HCofThisEvent* hCof = theEvent->GetHCofThisEvent();
		if (hCof != nullptr) {
			for (int i = 0; i < hCof->GetCapacity(); ++i) {
				G4VHitsCollection* hc = hCof->GetHC(i);
				if (hc == nullptr) continue;
				if (auto* sipmHitColl = dynamic_cast<G4SipmHitsCollection*>(hc)) {
					for (size_t k = 0; k < sipmHitColl->GetSize(); ++k) {
						G4SipmHit* hit = (*sipmHitColl)[k];
						sipmHitTimes.push_back(hit->getTime() / CLHEP::ns);
					}
					G4SipmPhotonHitCount += (G4int)sipmHitColl->GetSize();
				}
			}
		}
	}
#endif

	// SiPM hit summary with quantiles
	std::sort(sipmHitTimes.begin(), sipmHitTimes.end());
	int nHits = (int)sipmHitTimes.size();
	double firstTime = nHits > 0 ? sipmHitTimes[0] : NAN;
	double q10Time = nHits > 0 ? sipmHitTimes[(int)(0.1 * nHits)] : NAN;
	double q90Time = nHits > 0 ? sipmHitTimes[(int)(0.9 * nHits)] : NAN;

	h5Writer->addSipmHitSummary(EventID, nHits, firstTime, q10Time, q90Time);

#ifdef USE_G4SIPM
	// Probed unconditionally — Digitize() is a no-op when no g4sipm
	// digitizer modules are registered, and the digi-collection cast below
	// filters non-g4sipm entries.
	{
		// Run all digitizer modules
		G4DigiManager* digiManager = G4DigiManager::GetDMpointer();
		G4DCtable* dcTable = digiManager->GetDCtable();
		for (int i = 0; i < dcTable->entries(); i++) {
			G4String dmName = dcTable->GetDMname(i);
			G4VDigitizerModule* dm = digiManager->FindDigitizerModule(dmName);
			if (dm) {
				dm->Digitize();
			}
		}

		// Process digi collections
		G4DCofThisEvent* dCof = theEvent->GetDCofThisEvent();
		if (dCof != nullptr)
		{
			for (int i = 0; i < dCof->GetCapacity(); ++i)
			{
				G4VDigiCollection* dc = dCof->GetDC(i);
				if (dc == nullptr) continue;

				// Digi summary: count triggers by type
				if (auto* digiColl = dynamic_cast<G4SipmDigiCollection*>(dc))
				{
					int nPhoton = 0, nThermal = 0, nCrosstalk = 0, nAfterpulse = 0;
					for (int j = 0; j < (int)digiColl->GetSize(); ++j)
					{
						G4SipmDigi* digi = (*digiColl)[j];
						switch (digi->getType()) {
							case PHOTON:     nPhoton++;     break;
							case THERMAL:    nThermal++;    break;
							case CROSSTALK:  nCrosstalk++;  break;
							case AFTERPULSE: nAfterpulse++; break;
							default: break;
						}
					}
					h5Writer->addDigiSummary(EventID, nPhoton, nThermal, nCrosstalk, nAfterpulse);
				}

				// Voltage trace: extract a fixed 512-sample window starting 100ns before the 10% rising edge
				if (auto* traceColl = dynamic_cast<G4SipmVoltageTraceDigiCollection*>(dc))
				{
					constexpr int kTraceSamples = 512;
					constexpr double kPreTriggerNs = 100.0;

					for (int j = 0; j < (int)traceColl->GetSize(); ++j)
					{
						G4SipmVoltageTraceDigi* trace = (*traceColl)[j];
						double binWidth = trace->getTimeBinWidth();
						std::vector<double> fullTrace = trace->getContainer();
						int fullSize = (int)fullTrace.size();

						// Find the peak, then scan backwards to find the 10% rising edge
						auto peakIt = std::max_element(fullTrace.begin(), fullTrace.end());
						int peakIdx = (int)std::distance(fullTrace.begin(), peakIt);
						double peakV = *peakIt;

						// Estimate baseline from a quiet region (first 100 samples, before any signal)
						double baselineV = 0.0;
						int nBaseline = std::min(100, fullSize);
						for (int k = 0; k < nBaseline; k++) baselineV += fullTrace[k];
						baselineV /= nBaseline;

						double threshold10 = baselineV + 0.1 * (peakV - baselineV);

						// Scan backwards from peak to find where trace drops below 10% threshold
						int threshIdx = peakIdx;
						for (int k = peakIdx; k >= 0; k--) {
							if (fullTrace[k] < threshold10) {
								threshIdx = k + 1;
								break;
							}
							if (k == 0) threshIdx = 0;
						}

						// Start the window 100ns before the threshold crossing
						int preSamples = (int)(kPreTriggerNs * CLHEP::ns / binWidth);
						int startIdx = threshIdx - preSamples;
						if (startIdx < 0) startIdx = 0;
						if (startIdx + kTraceSamples > fullSize) startIdx = fullSize - kTraceSamples;
						if (startIdx < 0) startIdx = 0;

						int nSamples = std::min(kTraceSamples, fullSize - startIdx);
						std::vector<double> cropped(nSamples);
						for (int k = 0; k < nSamples; k++) {
							cropped[k] = fullTrace[startIdx + k] / CLHEP::volt;
						}

						double windowTMin = trace->time(startIdx) / CLHEP::ns;
						double windowTMax = trace->time(startIdx + nSamples - 1) / CLHEP::ns;

						h5Writer->addVoltageTrace(
							EventID,
							windowTMin,
							windowTMax,
							binWidth / CLHEP::ns,
							cropped
						);
					}
				}
			}
		}
	}
#endif

	// --- RunInformation updates (from saveDataToDataFile) ---
	G4int num_opticalPhotons = CerenkovPhotonsFromPrimary + scintiPhotonsFromPrimary + WLSPhotonsFromPrimary
	                         + CerenkovPhotonsFromSecondary + scintiPhotonsFromSecondary + WLSPhotonsFromSecondary;
	RunInformation->SetMinMaxValue_opticalPhotonsPerPrimary(num_opticalPhotons);
	RunInformation->SetMinMaxValue_opticalPhotonsPerEnergyDeposition(((G4double) num_opticalPhotons) / (deltaE_primary / CLHEP::MeV));
	RunInformation->SetMinMaxValue_opticalPhotonsAbsorbedInPhotonDetector(num_Absorbed_inPhotonDetector);

	// --- RunInformation updates (from saveDataToControlFile) ---
	G4int num_primary = EventInformation->GetNumberOfPrimaryParticles();
	RunInformation->IncNumberOfPrimaryParticles(num_primary);

	int opticalPhotonsFromPrimary = CerenkovPhotonsFromPrimary + scintiPhotonsFromPrimary + WLSPhotonsFromPrimary;
	int opticalPhotonsFromSecondary = CerenkovPhotonsFromSecondary + scintiPhotonsFromSecondary + WLSPhotonsFromSecondary;

	RunInformation->SetMinMaxValue_Control_opticalPhotons(opticalPhotonsFromPrimary + opticalPhotonsFromSecondary);
	RunInformation->SetMinMaxValue_Control_opticalPhotonsFromPrimary(opticalPhotonsFromPrimary);
	RunInformation->SetMinMaxValue_Control_opticalPhotonsFromSecondary(opticalPhotonsFromSecondary);
	RunInformation->SetMinMaxValue_Control_CerenkovPhotons(CerenkovPhotonsFromPrimary + CerenkovPhotonsFromSecondary);
	RunInformation->SetMinMaxValue_Control_ScintiPhotons(scintiPhotonsFromPrimary + scintiPhotonsFromSecondary);
	RunInformation->SetMinMaxValue_Control_WLSPhotons(WLSPhotonsFromPrimary + WLSPhotonsFromSecondary);
	RunInformation->SetMinMaxValue_Control_CerenkovPhotonsFromPrimary(CerenkovPhotonsFromPrimary);
	RunInformation->SetMinMaxValue_Control_ScintiPhotonsFromPrimary(scintiPhotonsFromPrimary);
	RunInformation->SetMinMaxValue_Control_WLSPhotonsFromPrimary(WLSPhotonsFromPrimary);
	RunInformation->SetMinMaxValue_Control_CerenkovPhotonsFromSecondary(CerenkovPhotonsFromSecondary);
	RunInformation->SetMinMaxValue_Control_ScintiPhotonsFromSecondary(scintiPhotonsFromSecondary);
	RunInformation->SetMinMaxValue_Control_WLSPhotonsFromSecondary(WLSPhotonsFromSecondary);

	RunInformation->SetMinMaxValue_Control_opticalPhotonsFromPrimaryPerPrimaryEnergyDeposition(((G4double) opticalPhotonsFromPrimary) / (deltaE_primary / CLHEP::MeV));
	RunInformation->SetMinMaxValue_Control_opticalPhotonsFromSecondaryPerPrimaryEnergyDeposition(((G4double) opticalPhotonsFromSecondary) / (deltaE_primary / CLHEP::MeV));
	RunInformation->SetMinMaxValue_Control_opticalPhotonsFromPrimaryPerPrimaryEnergyDeposition(((G4double) CerenkovPhotonsFromPrimary) / (deltaE_primary / CLHEP::MeV));
	RunInformation->SetMinMaxValue_Control_opticalPhotonsFromPrimaryPerPrimaryEnergyDeposition(((G4double) scintiPhotonsFromPrimary) / (deltaE_primary / CLHEP::MeV));
	RunInformation->SetMinMaxValue_Control_opticalPhotonsFromPrimaryPerPrimaryEnergyDeposition(((G4double) WLSPhotonsFromPrimary) / (deltaE_primary / CLHEP::MeV));
	RunInformation->SetMinMaxValue_Control_opticalPhotonsFromSecondaryPerPrimaryEnergyDeposition(((G4double) CerenkovPhotonsFromSecondary) / (deltaE_primary / CLHEP::MeV));
	RunInformation->SetMinMaxValue_Control_opticalPhotonsFromSecondaryPerPrimaryEnergyDeposition(((G4double) scintiPhotonsFromSecondary) / (deltaE_primary / CLHEP::MeV));
	RunInformation->SetMinMaxValue_Control_opticalPhotonsFromSecondaryPerPrimaryEnergyDeposition(((G4double) WLSPhotonsFromSecondary) / (deltaE_primary / CLHEP::MeV));

	G4int num_AbsorbedScinti_inFibre            = EventInformation->GetAbsorbedScintiPhotonInFibreCount();
	G4int num_AbsorbedCer_inFibre               = EventInformation->GetAbsorbedCerenkovPhotonInFibreCount();
	G4int num_AbsorbedWLS_inFibre               = EventInformation->GetAbsorbedWLSPhotonInFibreCount();
	G4int num_AbsorbedScinti_inPhotonDetector = EventInformation->GetAbsorbedScintiPhotonInPhotonDetectorCount();
	G4int num_AbsorbedCer_inPhotonDetector    = EventInformation->GetAbsorbedCerenkovPhotonInPhotonDetectorCount();
	G4int num_AbsorbedWLS_inPhotonDetector    = EventInformation->GetAbsorbedWLSPhotonInPhotonDetectorCount();
	G4int num_AbsorbedScinti_inScinti         = EventInformation->GetAbsorbedScintiPhotonInScintiCount();
	G4int num_AbsorbedCer_inScinti            = EventInformation->GetAbsorbedCerenkovPhotonInScintiCount();
	G4int num_AbsorbedWLS_inScinti            = EventInformation->GetAbsorbedWLSPhotonInScintiCount();

	G4int num_ScintiAbsorbed = num_AbsorbedScinti_inFibre + num_AbsorbedScinti_inPhotonDetector + num_AbsorbedScinti_inScinti;
	G4int num_CerAbsorbed = num_AbsorbedCer_inFibre + num_AbsorbedCer_inPhotonDetector + num_AbsorbedCer_inScinti;
	G4int num_WLSAbsorbed = num_AbsorbedWLS_inFibre + num_AbsorbedWLS_inPhotonDetector + num_AbsorbedWLS_inScinti;

	RunInformation->SetMinMaxValue_Control_opticalPhotonsAbsorbed_volume(num_Absorbed_inScinti, num_Absorbed_inFibre, num_Absorbed_inPhotonDetector, opticalPhotonsFromPrimary + opticalPhotonsFromSecondary);
	RunInformation->SetMinMaxValue_Control_opticalPhotonsAbsorbed_process(num_ScintiAbsorbed, scintiPhotonsFromPrimary + scintiPhotonsFromSecondary, num_CerAbsorbed, CerenkovPhotonsFromPrimary + CerenkovPhotonsFromSecondary, num_WLSAbsorbed, WLSPhotonsFromPrimary + WLSPhotonsFromSecondary);

	// --- Console output (event summary) ---
	if(!Messenger->GetQuiet())
	{
	G4cout << "primary particles:\t\t\t" << name_primary << " (" << num_primary << ")" << G4endl;
	G4cout << "primaryParticleID:\t\t\t" << ID_primary << G4endl;
	G4cout << "primaryParticle_pos/mm:\t\t\t" << initialPosition_primary / CLHEP::mm << G4endl;
	G4double particleMass_primary = RunInformation->GetParticleMass(name_primary);
	G4double initEkin_primary = sqrt(pow(particleMass_primary, 2) + pow(initialMomentum_primary.mag(), 2)) - particleMass_primary;
	G4cout << "primaryParticle_Ekin/MeV:\t\t" << initEkin_primary / CLHEP::MeV << G4endl;
	G4cout << "primaryParticle_dir:\t\t\t" << initialMomentum_primary.unit() << G4endl;
	G4cout << "primaryParticle_PathLength/mm:\t\t" << scintiPathLength_primary / CLHEP::mm << G4endl;
	G4cout << "primaryParticle_hit_time/ns:\t\t" << globalHitTime / CLHEP::ns << G4endl;
	G4cout << "primaryParticle_hit_pos/mm:\t\t" << scintiHitPoint_primary / CLHEP::mm << G4endl;
	G4double hitMomMag_primary = scintiHitMomentum_primary.mag();
	G4double hitEkin_primary = hitMomMag_primary > 0.0
		? sqrt(pow(particleMass_primary, 2) + pow(hitMomMag_primary, 2)) - particleMass_primary : 0.0;
	G4cout << "primaryParticle_hit_Ekin/MeV:\t\t" << hitEkin_primary / CLHEP::MeV << G4endl;
	G4cout << "primaryParticle_hit_dir:\t\t\t" << (hitMomMag_primary > 0.0 ? scintiHitMomentum_primary.unit() : G4ThreeVector(0,0,0)) << G4endl;
	G4cout << "primaryParticle_E_depos/MeV:\t\t" << deltaE_primary / CLHEP::MeV << G4endl;
	G4cout << "secondaryParticles_E_depos/MeV:\t\t" << totalSecondaryEnergy / CLHEP::MeV << G4endl;

	G4cout << "" << G4endl;
	G4cout << "secondary particles:\t\t\t";
	std::map<G4String, G4int>::iterator mapIter = names_secondary.begin();
	while (mapIter != names_secondary.end())
	{
		G4String name = mapIter->first;
		G4int number = mapIter->second;
		G4cout << name << " (" << number << ")";
		mapIter++;
		if (mapIter != names_secondary.end()) G4cout << "; ";
	}
	G4cout << G4endl;

	G4cout << "production processes:\t\t\t";
	mapIter = names_processes.begin();
	while (mapIter != names_processes.end())
	{
		G4String name = mapIter->first;
		G4int number = mapIter->second;
		G4cout << name << " (" << number << ")";
		mapIter++;
		if (mapIter != names_processes.end()) G4cout << "; ";
	}
	G4cout << G4endl;

	// Non-optical secondary particle details
	G4cout << "non-optical secondary particle details:" << G4endl;
	for (int iter2 = 1; iter2 < numParticles + 1; iter2++)
	{
		G4String particleName = EventInformation->GetParticleName(iter2);
		G4bool isPrimary = EventInformation->GetParticleIsPrimary(iter2);

		if (!isPrimary && particleName != G4OpticalPhoton::OpticalPhotonDefinition()->GetParticleName())
		{
			G4int trackID = iter2;
			G4int parentTrackID = EventInformation->GetParentTrackID(iter2);
			G4String productionMechanism = EventInformation->GetProductionMechanism(iter2);
			G4ThreeVector initialMomentum = EventInformation->GetInitialMomentum(iter2);
			G4double particleMass = RunInformation->GetParticleMass(particleName);
			G4double initialEnergy = sqrt(pow(particleMass, 2) + pow(initialMomentum.mag(), 2)) - particleMass;
			G4double hitTime = GoddessDataStorage->GetScintillatorHitTime(iter2);
			G4ThreeVector hitPoint = GoddessDataStorage->GetScintillatorHitPoint(iter2);
			G4double deltaE = GoddessDataStorage->GetEnergyDepositionInScintillator(iter2);

			G4cout << "  " << particleName << " (trackID: " << trackID << ", parentTrackID: " << parentTrackID << ", process: " << productionMechanism << ")"
			       << "  E_init/MeV: " << initialEnergy / CLHEP::MeV
			       << "  t_hit/ns: " << hitTime / CLHEP::ns
			       << "  pos_hit/mm: " << hitPoint / CLHEP::mm
			       << "  E_depos/MeV: " << deltaE / CLHEP::MeV << G4endl;
		}
	}
	} // end if(!Quiet)
}



void EventAction::printEventSummary()
{
	if(Messenger->GetQuiet()) return;

	G4int num_ScintiPhotons                       = EventInformation->GetScintiPhotonCount();
	G4int num_ScintiPhotons_byPrimary             = EventInformation->GetScintiPhotonByPrimaryCount();
	G4int num_ScintiPhotons_bySecondary           = EventInformation->GetScintiPhotonBySecondaryCount();
	G4int num_CerPhotons                          = EventInformation->GetCerenkovPhotonCount();
	G4int num_CerPhotons_byPrimary                = EventInformation->GetCerenkovPhotonByPrimaryCount();
	G4int num_CerPhotons_bySecondary              = EventInformation->GetCerenkovPhotonBySecondaryCount();
	G4int num_WLSPhotons                          = EventInformation->GetWLSPhotonCount();
	G4int num_Absorbed                            = EventInformation->GetAbsorbedCount();
	G4int num_AbsorbedScinti                      = EventInformation->GetAbsorbedScintiPhotonCount();
	G4int num_AbsorbedCer                         = EventInformation->GetAbsorbedCerenkovPhotonCount();
	G4int num_AbsorbedWLS                         = EventInformation->GetAbsorbedWLSPhotonCount();
	G4int num_Absorbed_inFibre                    = EventInformation->GetAbsorbedInFibreCount();
	G4int num_AbsorbedScinti_inFibre              = EventInformation->GetAbsorbedScintiPhotonInFibreCount();
	G4int num_AbsorbedCer_inFibre                 = EventInformation->GetAbsorbedCerenkovPhotonInFibreCount();
	G4int num_AbsorbedWLS_inFibre                 = EventInformation->GetAbsorbedWLSPhotonInFibreCount();
	G4int num_Absorbed_inPhotonDetector           = EventInformation->GetAbsorbedInPhotonDetectorCount();
	G4int num_AbsorbedScinti_inPhotonDetector     = EventInformation->GetAbsorbedScintiPhotonInPhotonDetectorCount();
	G4int num_AbsorbedCer_inPhotonDetector        = EventInformation->GetAbsorbedCerenkovPhotonInPhotonDetectorCount();
	G4int num_AbsorbedWLS_inPhotonDetector        = EventInformation->GetAbsorbedWLSPhotonInPhotonDetectorCount();
	G4int num_Absorbed_inScinti                   = EventInformation->GetAbsorbedInScintiCount();
	G4int num_AbsorbedScinti_inScinti             = EventInformation->GetAbsorbedScintiPhotonInScintiCount();
	G4int num_AbsorbedCer_inScinti                = EventInformation->GetAbsorbedCerenkovPhotonInScintiCount();
	G4int num_AbsorbedWLS_inScinti                = EventInformation->GetAbsorbedWLSPhotonInScintiCount();

	G4cout << "" << G4endl;

	G4cout << "Number of optical photons:" << G4endl;
	G4cout << "total:                 " << num_ScintiPhotons + num_CerPhotons + num_WLSPhotons << G4endl;
	G4cout << "by scintillation:      " << num_ScintiPhotons << "\t(by primary: " << num_ScintiPhotons_byPrimary << "\tby secondary: " << num_ScintiPhotons_bySecondary << ")" << G4endl;
	G4cout << "by Cerenkov radiation: " << num_CerPhotons << "\t(by primary: " << num_CerPhotons_byPrimary << "\tby secondary: " << num_CerPhotons_bySecondary << ")" << G4endl;
	G4cout << "by WLS:                " << num_WLSPhotons << G4endl << G4endl;

	G4cout << "Number of optical photons absorbed:" << G4endl;
	G4cout << "total:           " << num_Absorbed << "\t(scintillation photon: " << num_AbsorbedScinti << "\tCerenkov photon: " << num_AbsorbedCer << "\tWLS photon: " << num_AbsorbedWLS << ")" << G4endl;
	G4cout << "in fibre:        " << num_Absorbed_inFibre << "\t(scintillation photon: " << num_AbsorbedScinti_inFibre << "\tCerenkov photon: " << num_AbsorbedCer_inFibre << "\tWLS photon: " << num_AbsorbedWLS_inFibre << ")" << G4endl;
	if (G4SipmPhotonHitCount > 0) {
		G4cout << "in SiPM:         " << G4SipmPhotonHitCount << "\t(via G4SiPM sensitive detector)" << G4endl;
	} else {
		G4cout << "in SiPM:         " << num_Absorbed_inPhotonDetector << "\t(scintillation photon: " << num_AbsorbedScinti_inPhotonDetector << "\tCerenkov photon: " << num_AbsorbedCer_inPhotonDetector << "\tWLS photon: " << num_AbsorbedWLS_inPhotonDetector << ")" << G4endl;
	}
	G4cout << "in scintillator, wrapping, optical cement,...: " << num_Absorbed_inScinti << "\t(scintillation photon: " << num_AbsorbedScinti_inScinti << "\tCerenkov photon: " << num_AbsorbedCer_inScinti << "\tWLS photon: " << num_AbsorbedWLS_inScinti << ")" << G4endl << G4endl;

	G4cout << "-----------------------------------------------------------------------------" << G4endl << G4endl;
}
