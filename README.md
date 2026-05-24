# G4ScintKit

A Geant4 add-on toolkit for scintillator + SiPM detectors. Bundles the
GODDESS C++ library, the G4SiPM digitization library, a generic Geant4
application driven by a flat geometry manifest, and a Julia front-end
(`G4ScintKit.jl`) that builds manifests from high-level detector
specifications.

This package builds upon the following work:
- [GODDeSS](https://git.rwth-aachen.de/3pia/forge/goddess-package.) -- Geant4 Simulation package for scintillator based detectors, originally written by Erik Dietz-Laursonn.
- [G4SiPM](https://github.com/ntim/g4sipm/tree/master) -- Geant4 simulation toolkit for silicon photomultipliers, originally written by Tim Niggemann.

Since some modifications were made to integrate these packages into the broader package scope, this project links against forks, as described below. 


## Layout

- `goddess-package/` — GODDESS C++ library (git submodule, kcarloni/goddess-package fork).
- `g4sipm/` — G4SiPM C++ library (git submodule, kcarloni/g4sipm fork).
- `g4scintkit/` — Geant4 application: generic `PlaceManifest` interpreter + HDF5 writer.
- `G4ScintKit.jl/` — Julia package: manifest representation, geometry checks, fibre routing, builder helpers, run wrappers, HDF5 readers.
- `bash_scripts/` — build / run helpers.

## Prerequisites

The following packages are prerequisites: 

- **Geant4** built somewhere on disk. The kit was developed against 10.6 but
  newer versions should work — `find_package(Geant4)` is unpinned.
- **CMake** ≥ 3.5.
- **Boost** (the `regex` component).
- **HDF5** with C++ bindings.
- **zlib**.
- **Julia** ≥ 1.10 (for `G4ScintKit.jl`).

## Setup

```bash
# 1. Clone with submodules
git clone --recurse-submodules <your-fork-url> G4ScintKit
cd G4ScintKit

# 2. Point at your Geant4 install — either:
#    (a) export it in your shell (recommended)
export GEANT4_INSTALL_DIR=/path/to/your/geant4-install
#    (b) or copy the example and edit one line:
cp bash_scripts/setup_paths.sh.example bash_scripts/setup_paths.sh
#       (the live setup_paths.sh is gitignored)

# 3. Configure + build
bash_scripts/1_setup.sh      # cmake — builds g4sipm if needed, then g4scintkit
bash_scripts/2_compile.sh    # make g4scint

# 4. Smoke test (needs a manifest — use a bundled example)
bash_scripts/run_visu.sh --manifest <(julia --project=G4ScintKit.jl -e '
    using AstroParticleUnits; using G4ScintKit
    include("G4ScintKit.jl/examples/designs/B3.jl")
    write_manifest(stdout, build_manifest(B3Spec()))')
```

After step 3 you should have `build/g4scintkit/g4scint` as an executable.
`bash_scripts/setup_paths.sh.example` derives everything else (project
root, GODDESS, G4SIPM, SIMDIR, BUILDDIR) from its own location — no other
edits required.

## Driving the simulation:

The full simulation can be run end-to-end from Julia. For example, from the repo root:

```julia
using Pkg; Pkg.activate("G4ScintKit.jl")
using AstroParticleUnits
using G4ScintKit

include("G4ScintKit.jl/examples/designs/B3.jl")
spec = B3Spec()

# (a) Open the visualizer
run_visu(spec)

# (b) Run a batch simulation, writing HDF5 + log.txt + run_info.txt under outdir
outdir = run_simulation(spec;
    outdir = "output/B3_test",
    nevents = 10,
    injparticle = "mu-",
    injenergy = "3_GeV",   # underscore = space; run.sh substitutes
    trackphotons = false,
)
```

`run_simulation` accepts a `DetectorSpec`, a `GeometryManifest`, or a path
to an existing manifest file. See [G4ScintKit.jl/examples/](G4ScintKit.jl/examples/)
for a runnable script.

Additionally, given a manifest, the simulation can also be run directly from the command line
using the bash scripts:

```bash
bash_scripts/run_test.sh --manifest path/to/your.manifest --nevents 10
bash_scripts/run_visu.sh --manifest path/to/your.manifest
bash g4scintkit/run.sh --help            # full flag reference
bash g4scintkit/run.sh --manifest ... --outdir ... --dryrun
```

Re-running against a partially-completed `--outdir` resumes from the next event.

## Reading output

After `run_simulation` (or `run.sh`) finishes, `outdir` contains:

```
outdir/
├── Data/                     # C++ HDF5 output (one file per --filenamephrase)
├── Input/                    # Generated Run.init + GPS.mac (regenerated each run)
├── ControlData/inj.data      # Per-event injection record (used for resume logic)
├── log.txt                   # tee'd stdout/stderr of the g4scint run
└── run_info.txt              # Provenance: argv, git SHAs, manifest SHA256, host, date
```

The C++ side writes HDF5 under `<outdir>/Data/`. The headline API is `load`,
which reads every `.h5` file in `<outdir>/Data/`, walks every `g4run_*`
group, attaches `Unitful` units, and concatenates rows.

```julia
julia> out = load( outdir; )
SimulationOutput with 4 groups:
  input                 10 rows × 11 cols
  particle_hits         10 rows ×  6 cols
  optical_photons       10 rows × 11 cols
  sipm_hits             10 rows ×  6 cols
```



## Troubleshooting

- **First-time clone is missing submodules** — `git submodule update --init --recursive`.
