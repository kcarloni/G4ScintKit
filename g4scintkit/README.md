# g4scintkit

Generic Geant4 application for scintillator + SiPM detectors. Builds the
`g4scint` executable, which interprets a flat geometry manifest (produced
by the Julia `G4ScintKit.jl` `DetectorSpec` layer or hand-written) and
runs the simulation, writing event-level HDF5 output via `HDF5Writer`.

Links against the GODDESS C++ library (`../goddess-package`) and
optionally G4SiPM (`../g4sipm`) for SiPM digitisation.