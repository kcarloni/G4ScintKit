# G4ScintKit

A Geant4 toolkit for simulating plastic-scintillator detectors read out by
wavelength-shifting fibres and SiPMs.

It is built in three layers:

- **GODDeSS** and **G4SiPM** supply the optical physics primitives —
  scintillator tiles, reflective wrappings, fibres, optical couplings, SiPM
  response.
- **`g4scint`** (in `g4scintkit/`) is a single generic Geant4 application. It
  builds whatever geometry a *manifest* describes and writes event-level HDF5.
  There is no per-detector C++ to write.
- **`G4ScintKit.jl`** is an optional Julia front-end that generates manifests
  from high-level detector specifications, and reads the HDF5 output back.

The geometry manifest is the geometry input to `g4scint`: a flat text file
listing every scintillator, fibre, wrapping and photon detector to place, in
placement order. The manifest is the only way to define geometry — there is no
compiled-in alternative — and it is also where the Julia layer hands off to
the C++ one.

Because a manifest is plain text and fully determines the detector, the
simulation is usable from C++ and bash alone. Manifests are also small and
portable — material paths are recorded relative to the GODDeSS package root,
not to any one machine — so a detector design can be committed to version
control and shared, and every run records the SHA256 of the manifest it used.

The Julia interface is the practical way to generate manifests for new
designs. Hand-writing one is entirely reasonable for a simple geometry (see
docs/manifest_format.md), but it gets demanding
as designs grow: placement order is load-bearing, fibre routing means solving
for bend angles and axes, and the optical coupling has to be placed against
the right volume. `G4ScintKit.jl` handles the routing and checks the result
for fibre clashes and volume overlaps before you ever launch Geant4.

This package builds upon the following work:
- [GODDeSS](https://git.rwth-aachen.de/3pia/forge/goddess-package) -- Geant4 simulation package for scintillator based detectors, originally written by Erik Dietz-Laursonn.
- [G4SiPM](https://github.com/ntim/g4sipm) -- Geant4 simulation toolkit for silicon photomultipliers, originally written by Tim Niggemann.


## Layout

This repository is a bundle. Three of its subdirectories are git submodules — pinned here
at known-good commits. Cloning recursively (see [Setup](#setup)) gets all of
them at the right versions.

| Directory | What it is | Separate repo? |
|---|---|---|
| `goddess-package/` | GODDeSS C++ library: scintillator, fibre, wrapping and photon-detector primitives | yes — [kcarloni/goddess-package](https://github.com/kcarloni/goddess-package) |
| `g4sipm/` | G4SiPM C++ library: SiPM response and digitization | yes — [kcarloni/g4sipm](https://github.com/kcarloni/g4sipm) |
| `G4ScintKit.jl/` | Julia package: manifest representation, geometry checks, fibre routing, builder helpers, run wrappers, HDF5 readers | yes — [kcarloni/G4ScintKit.jl](https://github.com/kcarloni/G4ScintKit.jl) |
| `g4scintkit/` | The Geant4 application itself: builds `g4scint`, the generic manifest interpreter + HDF5 writer. Also holds `examples/` | no — lives in this repo |
| `bash_scripts/` | Build and run helpers | no |
| `docs/` | Geometry-manifest format reference and placement rules | no |


#### Note: The two C++ libraries are forks of older (not maintained) packages. 

`goddess-package/` and `g4sipm/` are forks carrying fixes this project depends
on, so they are **not interchangeable with their upstreams**:

- GODDeSS is originally the work of Erik Dietz-Laursonn (RWTH Aachen) and is
  licensed CC BY-NC-SA 3.0. See
  [`goddess-package/ATTRIBUTION.md`](goddess-package/ATTRIBUTION.md) for the
  full provenance — what is upstream, what was inherited from elsewhere, and
  what was changed locally — plus the licence terms.
- G4SiPM is originally the work of Tim Niggemann and is licensed GPLv3.

Do not repoint either submodule at its upstream; see
[Troubleshooting](#troubleshooting) for what breaks if you do.

## Prerequisites

### For the C++ build

- **Geant4**, built and installed somewhere on disk. Developed against 10.6;
  newer versions should work, as `find_package(Geant4)` is unpinned.
  Note that the build asks for every UI and visualisation driver by default
  (`find_package(Geant4 REQUIRED ui_all vis_all)`), so your Geant4 must have
  been built with them. If it was not, configure with
  `-DWITH_GEANT4_UIVIS=OFF` — you will lose visualization capability (`run_visu.sh`) but batch runs are
  unaffected.
- **CMake** ≥ 3.5.
- **Boost** — the `regex` and `program_options` components.
- **HDF5** with C++ bindings.
- **zlib**.
- **doxygen**, but only if you build the default `all` target.
  `bash_scripts/2_compile.sh` builds the `g4scint` target specifically and does
  not need it; a bare `make` does.

CLHEP ships with Geant4 and does not need installing separately. **ROOT and
SQLite are not required**, even though g4sipm's own README assumes them — this
bundle configures g4sipm with `-DWITH_ROOT=OFF -DWITH_SQLITE=OFF`.

`g4scint` runs single-threaded (plain `G4RunManager`, not the multithreaded
one), so scale up by running independent jobs rather than by threading.

### Additionally, for the Julia layer

- **Julia** ≥ 1.12 — `G4ScintKit.jl`'s `[compat]` stdlib bounds require it.

Skip this if you only intend to drive the simulation from manifests.

## Setup

```bash
# 1. Clone with submodules
git clone --recurse-submodules https://github.com/kcarloni/G4ScintKit.git
cd G4ScintKit

# 2. Point at your Geant4 install — either:
#    (a) export it in your shell (recommended)
export GEANT4_INSTALL_DIR=/path/to/your/geant4-install
#    (b) or copy the example and edit the one line at the top:
cp bash_scripts/setup_paths.sh.example bash_scripts/setup_paths.sh
#       (the live setup_paths.sh is gitignored, so it is yours to keep)

# 3. Configure + build
bash_scripts/1_setup.sh      # cmake — builds g4sipm if needed, then g4scintkit
bash_scripts/2_compile.sh    # make g4scint

# 4. Smoke test — runs a bundled example geometry, no Julia needed
bash_scripts/run_test.sh --nevents 10
```

Comments:

- On step 2: Every script here sources
`bash_scripts/setup_paths.sh`, falling back to `setup_paths.sh.example` when you
have not made your own copy. That file sets `GEANT4_INSTALL_DIR` only if it is
not already set, which is why exporting it in your shell is enough — you do not
have to create `setup_paths.sh` at all. Everything else it needs (the project
root, and `GODDESS`, `G4SIPM`, `SIMDIR`, `BUILDDIR`) it derives from its own
location, so there is nothing further to edit.

- Step 3 does the build work: `1_setup.sh` builds g4sipm out-of-tree first, then
configures the main project (GODDeSS compiles in-tree as a CMake
subdirectory); `2_compile.sh` then builds the `g4scint` target specifically.
Afterwards you should have `build/g4scintkit/g4scint` as an executable.

- If step 4 prints a scintillation-photon count, the whole chain works and you can
go to [C++ only execution](#c-only-execution).

### Rebuilding from scratch

`1_setup.sh` decides whether to rebuild g4sipm by checking only whether
`libg4sipm` already *exists* — not what it was built against. So after
switching Geant4 versions you must clear both build trees, or you will keep a
stale g4sipm linked against the old Geant4:

```bash
rm -rf build g4sipm/build     # or: bash_scripts/3_wipe_build.sh
bash_scripts/1_setup.sh
bash_scripts/2_compile.sh
```

## C++ only execution

Everything in this section needs **no Julia** — only the built `g4scint` binary
and bash. The geometry comes from a manifest file;  two working manifests ship
with this repo.

### Running

The wrapper scripts set up the environment for you and work from any shell:

```bash
# batch run into an output directory
bash_scripts/run_test.sh --manifest g4scintkit/examples/single_bar.manifest --nevents 10

# same geometry, opened in the Geant4 visualiser
bash_scripts/run_visu.sh --manifest g4scintkit/examples/single_bar.manifest
```

`run_test.sh` defaults to `single_bar.manifest` if you pass no `--manifest`, so
a bare `bash_scripts/run_test.sh --nevents 10` is the quickest check that the
build works. It always writes to `output/test/`, wiping it first.

To choose your own output directory, call `run.sh` directly. It needs the
environment variables that `setup_paths.sh` exports, so source that first:

```bash
source bash_scripts/setup_paths.sh

bash g4scintkit/run.sh \
    --outdir   output/single \
    --manifest g4scintkit/examples/single_bar.manifest \
    --nevents  10
```

Useful flags — `bash g4scintkit/run.sh --help` lists them all, and every
long option accepts either `--flag value` or `--flag=value`:

| Flag | Meaning |
|---|---|
| `--manifest <file>` | Geometry. Required; there is no default |
| `--outdir <dir>` | Where output goes. Re-running against a partially-complete directory resumes from the next event |
| `--nevents <N>` | Event count. `0` means interactive/visualiser |
| `--injparticle <name>` | `mu-`, `e-`, `gamma`, … (default `mu-`) |
| `--injenergy "<E unit>"` | e.g. `"3 GeV"` (default). Keep the quotes — the value contains a space |
| `--injpos "<x y z unit>"` | Centre of the injection plane, in world coordinates (default `"0 200 0 mm"`) |
| `--injdir "<x y z>"` | Injection direction (default `"0 -1 0"`) |
| `--trackphotons true\|false` | Track optical photons (default `false`). While off, the SiPM records nothing — scintillation photons are counted but not propagated — and turning it on is ~70× slower |
| `--seed <int\|clock>` | Random seed (default `12345`), so runs are reproducible by default |
| `--dryrun` | Write and print `Run.init`, `GPS.mac` and `run_info.txt`, then exit without simulating |

### The bundled geometries

| File | Geometry |
|---|---|
| `single_bar.manifest` | One 49.5 × 9.5 × 1875 mm scintillator bar with TiO2 wrapping, read out by two WLS fibres that route around to a shared SiPM at the front. 1 scint, 10 fibre segments, 1 SiPM. |
| `multi_bar.manifest` | Four such bars side by side, each with two fibres, looping at the back and meeting at one bundled SiPM at the front. 4 scint, 60 fibre segments, 1 SiPM. Note: this geometry has a 1mm gap at the central x=0 axis , so to test it you should offset the injection beam in `x`, see below.  |

The bundled geometries record their material paths *relative* to the GODDeSS package root, so
they run on any checkout without editing.

Both geometries lay their bars out long in z and thin in y, centred on the origin, which
is why the default injection sits above the detector at `y = +200 mm` and fires
straight down. `--injpos` moves the source plane in that global frame.

Note that the multi-bar geometry has four bars sit at x = −75.75,
−25.25, +25.25 and +75.75 mm and are 49.5 mm wide, leaving a **1 mm gap centred
on x = 0** — exactly where the default injection aims. A default run threads
that gap and deposits nothing at all, so offset the beam to hit a bar:

```bash
bash_scripts/run_test.sh \
    --manifest g4scintkit/examples/multi_bar.manifest \
    --injpos   "25.25 200 0 mm" \
    --nevents  10
```

A muon crossing one bar reports `primaryParticle_PathLength/mm: 9.5` — the bar
thickness — and some tens of thousands of scintillation photons. A path length
of `nan` means the beam missed the scintillator entirely.

Both files are generated from the `B3Spec` / `B4Spec` designs in
`G4ScintKit.jl/examples/designs/`. Regenerate them with
`julia --project=G4ScintKit.jl g4scintkit/examples/generate_examples.jl` if needed.
### Writing your own geometry

See [`docs/manifest_format.md`](docs/manifest_format.md) for the format, a
verified minimal example to copy, and the rules that will bite you.

## Julia based execution

`G4ScintKit.jl` sits on top of everything in the previous section. It does not
replace the C++ path — it *generates* the manifests that path consumes, and adds
three things worth having when you are designing a detector rather than simulating
a pre-existing one:

- **Detector specifications.** A `DetectorSpec` describes a design in physical,
  unitful terms (bar dimensions, materials, fibre choices) and
  `build_manifest(spec)` turns it into a manifest.
- **Fibre routing.** Looping a fibre around a bar end means solving for bend
  angles and axes; `route_fiber` / `add_fiber_path!` do that arithmetic.
- **Pre-flight geometry checks.** `check_geometry` looks for fibre-fibre
  clashes and scintillator overlaps. Neither stops Geant4, so these would
  otherwise surface only as wrong light propagation or mis-attributed energy
  deposits in a run that looks perfectly healthy. Geant4's own overlap search
  is off by default, reports what it does find as a *warning* rather than an
  error, and skips fibres altogether (GODDeSS disables that check as too slow
  for `G4Fibre`) — so for fibre clashes there is no downstream check at all.

### Setting up the environment

The Julia package has its own project, which needs instantiating once:

```bash
julia --project=G4ScintKit.jl -e 'using Pkg; Pkg.instantiate()'
```

Two of its dependencies (`AstroParticleUnits.jl`, `StructArrayTables.jl`) are
unregistered and resolved by git URL, so this step needs network access the
first time.

### Running a bundled design

`G4ScintKit.jl/examples/example.jl` is a runnable script covering the whole
cycle. The essentials, from the repository root:

```julia
using Pkg; Pkg.activate("G4ScintKit.jl")
using AstroParticleUnits
using G4ScintKit

# B3: one bar, two fibres meeting at a shared SiPM
include("G4ScintKit.jl/examples/designs/B3.jl")
spec = B3Spec()

manifest = build_manifest(spec)     # the same text file the C++ side reads
check_geometry(manifest)            # throws on fibre clashes / scint overlaps
fiber_lengths(manifest)             # design quantities, without simulating

# (a) Open the visualiser (output goes to output/visu, wiped each call)
run_visu(spec)

# (b) Batch run, writing HDF5 + log.txt + run_info.txt under outdir
outdir = run_simulation(spec;
    outdir      = "output/B3_test",
    nevents     = 10,
    injparticle = "mu-",
    injenergy   = 3u"GeV",
    trackphotons = false,
)
```

Keyword arguments beyond `outdir` are forwarded to `run.sh` as `--key=value`
pairs, so anything in the [flag table](#running) above works here too. Pass
physical quantities as Unitful values — `injenergy = 3u"GeV"`, not a string —
and they are converted to Geant4-internal units for you.

`run_simulation` and `run_visu` both accept a `DetectorSpec`, a
`GeometryManifest`, or a path to an existing manifest file, so you can also use
them to run a hand-written manifest.

### Detector specifications

A **spec** is a small keyword-constructible struct that holds the physical
parameters of one detector design — bar dimensions, casing thicknesses, and
which materials to use. It subtypes `DetectorSpec`, and each design pairs its
struct with a `build_manifest` method that turns those parameters into actual
placements. The spec is the design's dial panel; `build_manifest` is the design.

`B3Spec` and `B4Spec` (in `G4ScintKit.jl/examples/designs/`) both expose the
same knobs:

| Field | Default | Meaning |
|---|---|---|
| `scint_width` | `4.95u"cm"` | bar cross-section width |
| `scint_thickness` | `0.95u"cm"` | bar thickness |
| `scint_length` | `1.875u"m"` | bar length |
| `aluminum_thickness` | `0.0u"mm"` | aluminium casing; `0` disables it |
| `lead_thickness` | `0.0u"mm"` | lead sheet; `0` disables it |
| `scint_material` | `"fermilab"` | `fermilab`, `bc404`, `bc408`, `bc452-2pb`, `bc452-5pb`, `bc452-10pb` |
| `wrap_material` | `"tio2"` | `tio2`, `teflon`, `alu`, `bc620`, `tyvek` |
| `wls_material` | `"y11-300-r1"` | Kuraray `y11-*` and Saint-Gobain `bcf*` fibres, plus `eo534b` |
| `cement_material` | `"air1mm"` | `bc600`, `air`, `air1mm` |

So a variation on an existing design can be achieved with just keyword arguments:

```julia
# a thicker bar, wrapped in Teflon instead of TiO2, with the lead sheet on
spec = B3Spec(;
    scint_thickness = 2u"cm",
    wrap_material   = "teflon",
    lead_thickness  = 5u"mm",
)
manifest = check_geometry(build_manifest(spec))
```

Lengths are unitful, so `2u"cm"` and `20u"mm"` are interchangeable and you
cannot silently mix up millimetres and centimetres.

### Defining a new design

The two bundled designs are meant to be starting points — copy the one closest
to what you want into your own file and edit it. `B3.jl` is the simpler
(one bar, two fibres, a shared SiPM); `B4.jl` is the more instructive (four
bars, looped routing at the back, a bundled SiPM at the front).

Note that anything *not* in the table above lives inside `build_manifest`
rather than the spec. B4's bar count, for instance, is a local 
`num_bars = 4` — so going from four bars to six means editing the design file. This is the line between "a variation on this design"
and "a new design".

When writing a new design, use the `add_*!` helpers, which do the geometry arithmetic that is easy
to get wrong by hand. For example,  `add_scint_row!` lays out and spaces a row of bars,
`add_fiber_path!` and `route_fiber` solve for bend angles and axes, and
`add_inline_sipm!` derives the whole coupling geometry (`face_dir`, `rel_pos`,
`coupling_normal`, `coupling_pos`) from a fibre endpoint. Add placements in
construction order, as the ordering rule in
[`docs/manifest_format.md`](docs/manifest_format.md) requires, then
`to_manifest(b)` validates and emits.

Finally, `write_manifest(path, manifest)` gives you a portable file you can
commit and hand to someone with no Julia installed.

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

The C++ side writes HDF5 under `<outdir>/Data/`. The Julia-side API for reading these outputs is `load`,
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

Grouped by where things go wrong. Symptoms are quoted as they appear.

### Cloning and submodules

- **First-time clone is missing submodules** — `git submodule update --init --recursive`.
- **Never repoint `goddess-package/` or `g4sipm/` at their upstreams.** Both
  forks carry fixes this project depends on. Upstream g4sipm reintroduces a
  use-after-free in `MaterialFactory` that segfaults on `/run/beamOn` as soon as
  any `SIPM` line names a `model=`; upstream GODDeSS loses the `G4Fibre`
  out-of-range guard and the bounding-box pre-filter that makes placing many
  fibres tractable. Both upstreams are unmaintained, so these will not be fixed
  there.

### Building

- **`make` fails complaining about doxygen** — the default `all` target builds
  the GODDeSS documentation. Build the application target instead:
  `bash_scripts/2_compile.sh`, or `make g4scint`.
- **CMake cannot satisfy `find_package(Geant4 REQUIRED ui_all vis_all)`** —
  your Geant4 was built without UI/visualisation drivers. Configure with
  `-DWITH_GEANT4_UIVIS=OFF`; batch runs are unaffected, but you lose
  `run_visu.sh`.
- **Link or runtime errors after switching Geant4 versions** — you are almost
  certainly carrying a stale `libg4sipm` built against the old Geant4. See
  [Rebuilding from scratch](#rebuilding-from-scratch).

### Environment

- **`setup_paths: '<dir>' does not look like the G4ScintKit root`** — the
  script could not work out its own location. Set the root explicitly:
  `export G4SCINTKIT=/path/to/G4ScintKit`, then source it again.
- **`manifest: material path '...' is relative, but the GODDESS environment
  variable is not set`** — source `bash_scripts/setup_paths.sh` before running
  `g4scint` or `run.sh` directly. The wrapper scripts in `bash_scripts/` do
  this for you.
- **`DetectorConstruction: --manifest is required`** — geometry is never
  optional and there are no built-in setups. Pass a manifest; see
  [the bundled geometries](#the-bundled-geometries).

### Simulation 

- **`primaryParticle_PathLength/mm: nan`** — the
  beam missed the scintillator entirely. Check `--injpos` against your bar
  positions; a gap between bars sitting on the beam axis deposits exactly
  nothing. The multi-bar example has one at x = 0.
- **Energy deposits look fine, but `in SiPM: 0` and `WLS photon: 0`** — this is
  the *expected* result with the default settings, not a fault. Optical photon
  tracking is off unless you ask for it (`--trackphotons` defaults to `false`),
  so scintillation photons are counted where they are created but never
  propagated down the fibre. Pass `--trackphotons true` to see SiPM hits.
  Expect it to be far slower — roughly 70× in a single-bar test (0.8 s vs 57 s
  for 20 events) — which is why it is off by default.
- **Construction aborts with `G4Exception : InvalidSetup` from
  `G4OpticalCoupling`** — a fibre ends inside its mother volume while the SiPM
  sits beyond that end, leaving the optical coupling nowhere to be placed. The
  message names the three volumes that disagree. Extend the fibre's `end` past
  the face being read out and put the SiPM just beyond it.

### Julia

- **`Package AstroParticleUnits ... is required but does not seem to be
  installed`** — the environment has not been instantiated:
  `julia --project=G4ScintKit.jl -e 'using Pkg; Pkg.instantiate()'`.


