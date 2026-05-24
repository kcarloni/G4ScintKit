# G4ScintKit

A Geant4 add-on toolkit for scintillator + SiPM detectors. Bundles the
GODDESS C++ library, the G4SiPM digitization library, a generic
Geant4 application driven by a flat geometry manifest, and a Julia
front-end (`G4ScintKit.jl`) that builds manifests from high-level
detector specifications.

## Layout

- `goddess-package/` — GODDESS C++ library (git submodule, kcarloni/goddess-package fork).
- `g4sipm/` — G4SiPM C++ library (git submodule, kcarloni/g4sipm fork).
- `g4scintkit/` — Geant4 application: generic `PlaceManifest` interpreter + HDF5 writer.
- `G4ScintKit.jl/` — Julia package: manifest representation, geometry checks, fibre routing, builder helpers, run wrappers.
- `bash_scripts/` — build / run helpers.