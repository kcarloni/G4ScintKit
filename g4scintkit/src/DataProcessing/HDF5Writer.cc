/*
 * HDF5Writer.cc
 *
 * Scintillator + SiPM event HDF5 writer implementation.
 */

#include <HDF5Writer.hh>


HDF5Writer::HDF5Writer()
{
}

HDF5Writer::~HDF5Writer()
{
}

void HDF5Writer::clearAll()
{
	IN_EventId.clear();
	IN_Name.clear();
	IN_Pid.clear();
	IN_InitPosX.clear();
	IN_InitPosY.clear();
	IN_InitPosZ.clear();
	IN_InitEkin.clear();
	IN_InitDirX.clear();
	IN_InitDirY.clear();
	IN_InitDirZ.clear();

	PH_EventId.clear();
	PH_Edep.clear();
	PH_EdepPri.clear();
	PH_EdepSec.clear();
	PH_FirstHitTime.clear();

	OP_EventId.clear();
	OP_NCerPri.clear();
	OP_NScintiPri.clear();
	OP_NWlsPri.clear();
	OP_NCerSec.clear();
	OP_NScintiSec.clear();
	OP_NWlsSec.clear();
	OP_NAbsFibre.clear();
	OP_NAbsSipm.clear();
	OP_NAbsScinti.clear();

	SH_EventId.clear();
	SH_NHits.clear();
	SH_FirstTime.clear();
	SH_Q10Time.clear();
	SH_Q90Time.clear();

	DS_EventId.clear();
	DS_NPhoton.clear();
	DS_NThermal.clear();
	DS_NCrosstalk.clear();
	DS_NAfterpulse.clear();

	VT_EventId.clear();
	VT_TMin.clear();
	VT_TMax.clear();
	VT_TimeBinWidth.clear();
	VT_Voltages.clear();
}

// ---------- per-event accumulation ----------

void HDF5Writer::addInputParticle(
	int event_id,
	const std::string& name,
	int pid,
	double init_pos_x_mm,
	double init_pos_y_mm,
	double init_pos_z_mm,
	double init_kinetic_energy_MeV,
	double init_dir_x,
	double init_dir_y,
	double init_dir_z)
{
	IN_EventId.push_back(event_id);
	IN_Name.push_back(name);
	IN_Pid.push_back(pid);
	IN_InitPosX.push_back(init_pos_x_mm);
	IN_InitPosY.push_back(init_pos_y_mm);
	IN_InitPosZ.push_back(init_pos_z_mm);
	IN_InitEkin.push_back(init_kinetic_energy_MeV);
	IN_InitDirX.push_back(init_dir_x);
	IN_InitDirY.push_back(init_dir_y);
	IN_InitDirZ.push_back(init_dir_z);
}

void HDF5Writer::addParticleHits(
	int event_id,
	double edep_MeV,
	double edep_primaries_MeV,
	double edep_secondaries_MeV,
	double first_hit_time_ns)
{
	PH_EventId.push_back(event_id);
	PH_Edep.push_back(edep_MeV);
	PH_EdepPri.push_back(edep_primaries_MeV);
	PH_EdepSec.push_back(edep_secondaries_MeV);
	PH_FirstHitTime.push_back(first_hit_time_ns);
}

void HDF5Writer::addOpticalPhotonSummary(
	int event_id,
	int n_cerenkov_primary,
	int n_scinti_primary,
	int n_wls_primary,
	int n_cerenkov_secondary,
	int n_scinti_secondary,
	int n_wls_secondary,
	int n_absorbed_fibre,
	int n_absorbed_sipm,
	int n_absorbed_scinti)
{
	OP_EventId.push_back(event_id);
	OP_NCerPri.push_back(n_cerenkov_primary);
	OP_NScintiPri.push_back(n_scinti_primary);
	OP_NWlsPri.push_back(n_wls_primary);
	OP_NCerSec.push_back(n_cerenkov_secondary);
	OP_NScintiSec.push_back(n_scinti_secondary);
	OP_NWlsSec.push_back(n_wls_secondary);
	OP_NAbsFibre.push_back(n_absorbed_fibre);
	OP_NAbsSipm.push_back(n_absorbed_sipm);
	OP_NAbsScinti.push_back(n_absorbed_scinti);
}

void HDF5Writer::addSipmHitSummary(
	int event_id,
	int n_hits,
	double first_hit_time_ns,
	double q10_time_ns,
	double q90_time_ns)
{
	SH_EventId.push_back(event_id);
	SH_NHits.push_back(n_hits);
	SH_FirstTime.push_back(first_hit_time_ns);
	SH_Q10Time.push_back(q10_time_ns);
	SH_Q90Time.push_back(q90_time_ns);
}

void HDF5Writer::addDigiSummary(
	int event_id,
	int n_photon,
	int n_thermal,
	int n_crosstalk,
	int n_afterpulse)
{
	DS_EventId.push_back(event_id);
	DS_NPhoton.push_back(n_photon);
	DS_NThermal.push_back(n_thermal);
	DS_NCrosstalk.push_back(n_crosstalk);
	DS_NAfterpulse.push_back(n_afterpulse);
}

void HDF5Writer::addVoltageTrace(
	int event_id,
	double t_min_ns,
	double t_max_ns,
	double time_bin_width_ns,
	const std::vector<double>& voltages)
{
	VT_EventId.push_back(event_id);
	VT_TMin.push_back(t_min_ns);
	VT_TMax.push_back(t_max_ns);
	VT_TimeBinWidth.push_back(time_bin_width_ns);
	VT_Voltages.push_back(voltages);
}

// ---------- HDF5 writing ----------

void HDF5Writer::writeDatasets()
{
	// Skip if no data was accumulated (e.g. from a zero-event init run)
	if (IN_EventId.empty() && PH_EventId.empty() && OP_EventId.empty()
	    && SH_EventId.empty() && DS_EventId.empty() && VT_EventId.empty())
		return;

	std::string runGroupName = "g4run_" + std::to_string(RunId);
	H5::Group runGrp = createOrderedGroup(*File, runGroupName);

	// --- input (1 row per primary particle) ---
	if (!IN_EventId.empty()) {
		H5::Group grp = createOrderedGroup(runGrp, "input");
		writeIntDataset(grp, "g4event_id", IN_EventId);
		writeStringDataset(grp, "name", IN_Name);
		writeIntDataset(grp, "pid", IN_Pid);
		writeDoubleDataset(grp, "init_pos_x_mm", IN_InitPosX);
		writeDoubleDataset(grp, "init_pos_y_mm", IN_InitPosY);
		writeDoubleDataset(grp, "init_pos_z_mm", IN_InitPosZ);
		writeDoubleDataset(grp, "init_kinetic_energy_MeV", IN_InitEkin);
		writeDoubleDataset(grp, "init_dir_x", IN_InitDirX);
		writeDoubleDataset(grp, "init_dir_y", IN_InitDirY);
		writeDoubleDataset(grp, "init_dir_z", IN_InitDirZ);
	}

	// --- particle_hits (1 row per event) ---
	if (!PH_EventId.empty()) {
		H5::Group grp = createOrderedGroup(runGrp, "particle_hits");
		writeIntDataset(grp, "g4event_id", PH_EventId);
		writeDoubleDataset(grp, "edep_MeV", PH_Edep);
		writeDoubleDataset(grp, "edep_primaries_MeV", PH_EdepPri);
		writeDoubleDataset(grp, "edep_secondaries_MeV", PH_EdepSec);
		writeDoubleDataset(grp, "first_hit_time_ns", PH_FirstHitTime);
	}

	// --- optical_photons ---
	if (!OP_EventId.empty()) {
		H5::Group grp = createOrderedGroup(runGrp, "optical_photons");
		writeIntDataset(grp, "g4event_id", OP_EventId);
		writeIntDataset(grp, "n_cerenkov_primary", OP_NCerPri);
		writeIntDataset(grp, "n_scinti_primary", OP_NScintiPri);
		writeIntDataset(grp, "n_wls_primary", OP_NWlsPri);
		writeIntDataset(grp, "n_cerenkov_secondary", OP_NCerSec);
		writeIntDataset(grp, "n_scinti_secondary", OP_NScintiSec);
		writeIntDataset(grp, "n_wls_secondary", OP_NWlsSec);
		writeIntDataset(grp, "n_absorbed_fibre", OP_NAbsFibre);
		writeIntDataset(grp, "n_absorbed_sipm", OP_NAbsSipm);
		writeIntDataset(grp, "n_absorbed_scinti", OP_NAbsScinti);
	}

	// --- sipm_hits ---
	if (!SH_EventId.empty()) {
		H5::Group grp = createOrderedGroup(runGrp, "sipm_hits");
		writeIntDataset(grp, "g4event_id", SH_EventId);
		writeIntDataset(grp, "n_hits", SH_NHits);
		writeDoubleDataset(grp, "first_hit_time_ns", SH_FirstTime);
		writeDoubleDataset(grp, "q10_time_ns", SH_Q10Time);
		writeDoubleDataset(grp, "q90_time_ns", SH_Q90Time);
	}

	// --- sipm_digi_summary (G4SiPM only) ---
	if (!DS_EventId.empty()) {
		H5::Group grp = createOrderedGroup(runGrp, "sipm_digi_summary");
		writeIntDataset(grp, "g4event_id", DS_EventId);
		writeIntDataset(grp, "n_photon", DS_NPhoton);
		writeIntDataset(grp, "n_thermal", DS_NThermal);
		writeIntDataset(grp, "n_crosstalk", DS_NCrosstalk);
		writeIntDataset(grp, "n_afterpulse", DS_NAfterpulse);
	}

	// --- sipm_voltage_trace (G4SiPM only) ---
	if (!VT_EventId.empty()) {
		H5::Group grp = createOrderedGroup(runGrp, "sipm_voltage_trace");
		writeIntDataset(grp, "g4event_id", VT_EventId);
		writeDoubleDataset(grp, "t_min_ns", VT_TMin);
		writeDoubleDataset(grp, "t_max_ns", VT_TMax);
		writeDoubleDataset(grp, "time_bin_width_ns", VT_TimeBinWidth);
		write2DDoubleDataset(grp, "voltages_V", VT_Voltages);
	}

}
