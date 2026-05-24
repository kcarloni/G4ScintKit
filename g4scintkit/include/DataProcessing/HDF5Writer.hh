/*
 * HDF5Writer.hh
 *
 * Scintillator + SiPM event HDF5 writer.  Accumulates simulation data
 * and writes input, particle_hits, optical_photons, sipm_hits,
 * sipm_digi_summary, and sipm_voltage_trace tables.
 * Inherits generic I/O utilities from HDF5TableWriter (in GODDeSS).
 */

#ifndef HDF5WRITER_HH_
#define HDF5WRITER_HH_

#include <HDF5TableWriter.hh>
#include <string>
#include <vector>


class HDF5Writer : public HDF5TableWriter
{
public:
	HDF5Writer();
	~HDF5Writer() override;

	// ---- input (1 row per primary particle) ----
	void addInputParticle(
		int event_id,
		const std::string& name,
		int pid,
		double init_pos_x_mm,
		double init_pos_y_mm,
		double init_pos_z_mm,
		double init_kinetic_energy_MeV,
		double init_dir_x,
		double init_dir_y,
		double init_dir_z);

	// ---- particle_hits (1 row per event) ----
	void addParticleHits(
		int event_id,
		double edep_MeV,
		double edep_primaries_MeV,
		double edep_secondaries_MeV,
		double first_hit_time_ns);

	// ---- optical_photons (1 row per event) ----
	void addOpticalPhotonSummary(
		int event_id,
		int n_cerenkov_primary,
		int n_scinti_primary,
		int n_wls_primary,
		int n_cerenkov_secondary,
		int n_scinti_secondary,
		int n_wls_secondary,
		int n_absorbed_fibre,
		int n_absorbed_sipm,
		int n_absorbed_scinti);

	// ---- sipm_hits (1 row per event, aggregated) ----
	void addSipmHitSummary(
		int event_id,
		int n_hits,
		double first_hit_time_ns,
		double q10_time_ns,
		double q90_time_ns);

	// ---- sipm_digi_summary (1 row per event, G4SiPM only) ----
	void addDigiSummary(
		int event_id,
		int n_photon,
		int n_thermal,
		int n_crosstalk,
		int n_afterpulse);

	// ---- sipm_voltage_trace (1 row per event + 2D voltages, G4SiPM only) ----
	void addVoltageTrace(
		int event_id,
		double t_min_ns,
		double t_max_ns,
		double time_bin_width_ns,
		const std::vector<double>& voltages);

protected:
	void writeDatasets() override;
	void clearAll() override;

private:
	// --- input accumulators (1 row per primary) ---
	std::vector<int>         IN_EventId;
	std::vector<std::string> IN_Name;
	std::vector<int>         IN_Pid;
	std::vector<double>      IN_InitPosX;
	std::vector<double>      IN_InitPosY;
	std::vector<double>      IN_InitPosZ;
	std::vector<double>      IN_InitEkin;
	std::vector<double>      IN_InitDirX;
	std::vector<double>      IN_InitDirY;
	std::vector<double>      IN_InitDirZ;

	// --- particle_hits accumulators (1 row per event) ---
	std::vector<int>    PH_EventId;
	std::vector<double> PH_Edep;
	std::vector<double> PH_EdepPri;
	std::vector<double> PH_EdepSec;
	std::vector<double> PH_FirstHitTime;

	// --- optical_photons accumulators ---
	std::vector<int> OP_EventId;
	std::vector<int> OP_NCerPri;
	std::vector<int> OP_NScintiPri;
	std::vector<int> OP_NWlsPri;
	std::vector<int> OP_NCerSec;
	std::vector<int> OP_NScintiSec;
	std::vector<int> OP_NWlsSec;
	std::vector<int> OP_NAbsFibre;
	std::vector<int> OP_NAbsSipm;
	std::vector<int> OP_NAbsScinti;

	// --- sipm_hits accumulators ---
	std::vector<int>    SH_EventId;
	std::vector<int>    SH_NHits;
	std::vector<double> SH_FirstTime;
	std::vector<double> SH_Q10Time;
	std::vector<double> SH_Q90Time;

	// --- sipm_digi_summary accumulators ---
	std::vector<int> DS_EventId;
	std::vector<int> DS_NPhoton;
	std::vector<int> DS_NThermal;
	std::vector<int> DS_NCrosstalk;
	std::vector<int> DS_NAfterpulse;

	// --- sipm_voltage_trace accumulators ---
	std::vector<int>    VT_EventId;
	std::vector<double> VT_TMin;
	std::vector<double> VT_TMax;
	std::vector<double> VT_TimeBinWidth;
	std::vector<std::vector<double>> VT_Voltages;
};

#endif
