# g4sipm + G4ScintKit crash debugging

Session log of the May 2026 investigation into segfaults that appeared once
the per-SiPM g4sipm dispatch (C2) was wired through `--manifest`. The
original symptom was reported as

> enabling any g4sipm model segfaults Geant4 at `/run/beamOn`, deep inside
> `G4MaterialPropertiesTable::GetProperty(int, bool)` called from
> `G4Cerenkov::BuildThePhysicsTable`

That report turned out to be the **first** of three independent bugs.
We identified three; **two are fixed; one (bug B) is partially mitigated
and remains open**.

## TL;DR

| Bug | Where | Status |
|---|---|---|
| **A** | `DetectorConstruction::~DetectorConstruction` blast-deletes the global G4 material table at exit, double-freeing materials owned by g4sipm's `MaterialFactory` singleton. SIGSEGV at program teardown. | **Fixed** — destructor no longer deletes globals. |
| **B** | Double-owner UAF on BisphenolA's MPT. `MaterialFactory::getEpoxy` allocates an MPT and attaches it to BisphenolA. Per-SiPM model code then calls `new G4Material(name, density, BisphenolA)` (derived-material constructor) — `CopyPointersOfBaseMaterial` copies BisphenolA's MPT *pointer* into the derived windowMaterial. The subsequent `windowMaterial->SetMaterialPropertiesTable(mpt)` does `delete fMaterialPropertiesTable` first, which deletes BisphenolA's MPT. BisphenolA keeps the dangling pointer in `theMaterialTable`; the freed slot is reused by a later allocation (G4SipmHousing or its container G4LogicalVolume); `G4OpRayleigh::BuildPhysicsTable` later iterates BisphenolA and dereferences the now-overlaid bytes → SIGSEGV. | **Fixed** — see "Fix" below. |
| **C** | g4sipm + photon tracking on → `H5::H5Location::createDataSet` throws `GroupIException` at `RunAction::EndOfRunAction`. Dataset path or group handle is invalid. Co-located with bug B: same dangling MPT, different downstream consumer. | **Fixed** — resolved by the bug-B fix, as predicted. |

Plus a separable bug found during the hunt:

| Bug | Where | Status |
|---|---|---|
| **G4Fibre OOB** | `G4Fibre::ConstructFibreLayerLogical` reads `GrandMotherAndAuntVolumes[0]` after a previous layer cleared the vector. Container-overflow read past `size()`. ASan-deterministic; in bare runs it often returns the stale `nullptr` (benign), but sometimes returns wild bytes that crash downstream. | **Fixed** — guarded the read with `!empty()`. |

The "deep inside `G4MaterialPropertiesTable::GetProperty` from `G4Cerenkov`" line in the original bug report appears to refer to either bug A (which surfaced at teardown but was misread as a `BuildPhysicsTable` crash because of stdout buffering) or bug B (which surfaces in different optical processes depending on layout). The pre-existing empty-MPT patch on `g4sipm/g4sipm/src/MaterialFactory.cc` for `getBoronCarbideCeramic()`/`getCopper()` is benign under Geant4 10.6 (its `GetProperty(int)` is null-safe — see Geant4 source analysis below) — but neither necessary nor sufficient.

---

## Bug A — `DetectorConstruction::~DetectorConstruction` double-free

### Stack at crash (under lldb, Release build)

```
DetectorConstruction::~DetectorConstruction + 136          ← EXC_BAD_ACCESS
DetectorConstruction::~DetectorConstruction + 12  (thunk)
G4RunManager::DeleteUserInitializations
G4RunManager::~G4RunManager
main
```

Faulting instruction was `ldr x8, [x8, #0x8]; blr x8` — loading a vtable slot
from a freed `G4Material`. Register `x20` held `libG4materials.dylib`G4Material::theMaterialTable`,
confirming the loop was walking the global material table.

### Mechanism

`~DetectorConstruction` (header line 81) called:

```cpp
CleanUp();
DeleteMaterialPropertiesTables();
DeleteMaterials();
```

`DeleteMaterials` was:

```cpp
void DetectorConstruction::DeleteMaterials() {
    G4MaterialTable* matTable = (G4MaterialTable*) G4Material::GetMaterialTable();
    for (size_t i = 0; i < matTable->size(); i++) delete (*matTable)[i];
    matTable->clear();
}
```

That is a **blast-delete of every material in the global Geant4 table**,
including materials owned by other singletons. When `sipm_model` is set,
g4sipm's `MaterialFactory::getInstance()` registers Air, BoronCarbide, Copper,
Epoxy, Silicon, and per-model classes (e.g. `HamamatsuS12573100C`)
register their own epoxy-derived window materials. **All** of those end up
in the global table.

At program exit:
1. `~DetectorConstruction` deletes them all and `clear()`s the table.
2. `MaterialFactory::~MaterialFactory()` (`g4sipm/g4sipm/src/MaterialFactory.cc:29-34`)
   `delete`s the same pointers → double-free / UAF.

`/tracking/storeTrajectory 1` made the crash deterministic by adding enough
allocator pressure during the event that the freed material's memory was
reused before the destructor walked it, corrupting the vtable read.
Without `storeTrajectory`, the same UAF was present but the freed slabs
were still intact when read, so the program limped to exit silently.

GODDESS-PD path (`sipm_model=""`) did not crash because no second owner
(MaterialFactory) existed — only Geant4 itself, and `clear()` left
Geant4 with an empty table that it never re-iterated.

### Fix

`g4scintkit/include/Preparation/DetectorConstruction.hh:77-82`:

```cpp
~DetectorConstruction()
{
    // Materials and MPTs in G4Material::theMaterialTable are owned by
    // Geant4 (and, when g4sipm is in use, by g4sipm's MaterialFactory
    // singleton). Hand-deleting them here causes a double-free at exit.
    // Leak-at-exit is fine; the OS reclaims on process tear-down.
    CleanUp();
}
```

Function definitions for `DeleteMaterials` and `DeleteMaterialPropertiesTables`
removed entirely. Geant4 owns those by convention; hand-deletion was always
incorrect.

### Verification

Under lldb (Debug+ASan, Release+ASan, Release+lldb), program exits 0 with
`sipm_model="hamamatsu-s12573-100c"` and `/tracking/storeTrajectory 1` set.
Bare Release still hits **bug B** further upstream, but `~DetectorConstruction`
is no longer the crash site.

---

## Bug B — g4sipm × storeTrajectory MPT corruption

### Reproducer (deterministic on this machine, Release build)

Prerequisites:
- macOS arm64 (Apple Silicon)
- Clang 21.0 / Geant4 10.6.0 / HDF5 2.1.1
- G4ScintKit built in `build/` with default Release flags
- `BUILDDIR`, `G4SCINTKIT`, etc. exported via `bash_scripts/setup_paths.sh`

Build a B3 manifest that selects a g4sipm model:

```bash
cd /Users/kiara/home/research/tambo/G4TamboSim
julia --project=. -e '
  using G4ScintKit
  import G4ScintKit: build_manifest, write_manifest
  for f in readdir("designs"); include(joinpath(@__DIR__, "designs", f)); end
  spec = B3Spec(sipm_model="hamamatsu-s12573-100c")
  write_manifest("/tmp/b3_g4sipm.manifest", build_manifest(spec))
'
```

Prepare a minimal launch dir that includes `/tracking/storeTrajectory 1`
(mimics what `vis.mac` applies in `run_visu`):

```bash
source /Users/kiara/home/research/tambo/G4ScintKit/bash_scripts/setup_paths.sh
OUT=/tmp/bugB_repro; rm -rf "$OUT"; mkdir -p "$OUT/Input" "$OUT/Data"

cat > "$OUT/Input/GPS.mac" <<'EOF'
/gps/verbose 0
/gps/outFile inj.data
/gps/particle mu-
/gps/energy/eMin 3 GeV
/gps/energy/eMax 3 GeV
/gps/plane/pos 0 200 0 mm
/gps/plane/surfaceNormal 0 -1 0
/gps/plane/shape rect
/gps/plane/posDist uniform
/gps/plane/a 0 mm
/gps/plane/b 0 mm
/gps/angle/thetaMin 0 deg
/gps/angle/thetaMax 0 deg
/gps/angle/phiMin 0 deg
/gps/angle/phiMax 0 deg
EOF

echo "/tracking/storeTrajectory 1" > "$OUT/Input/Pre.mac"

cat > "$OUT/Input/Run.init" <<EOF
--batch 1
--particleSourceInput $OUT/Input/GPS.mac
--macro $OUT/Input/Pre.mac
--outDir $OUT
--seed 12345
--noOpticalPhotonTracking
--manifest /tmp/b3_g4sipm.manifest
EOF
```

Run:

```bash
"$BUILDDIR/g4scint" --init "$OUT/Input/Run.init"
# Pre-mitigation: exit 139 (SIGSEGV), 100% of the time.
# Post-mitigation (current main): exit 139 ~50% of the time; exit 0 otherwise.
```

### Crash signature (Release + lldb)

```
EXC_BAD_ACCESS (code=2, address=0x818400007)
  G4MaterialPropertiesTable::ConstPropertyExists(char const*) const + 168
  G4OpRayleigh::CalculateRayleighMeanFreePaths(G4Material const*) const + 96
  G4OpRayleigh::BuildPhysicsTable(G4ParticleDefinition const&) + 268
  G4VUserPhysicsList::BuildPhysicsTable(G4ParticleDefinition*)
  G4VUserPhysicsList::BuildPhysicsTable()
  G4RunManagerKernel::BuildPhysicsTables(bool)
  G4RunManagerKernel::RunInitialization(bool)
  G4RunManager::RunInitialization
  G4RunManager::BeamOn
  G4RunMessenger::SetNewValue
  G4UIcommand::DoIt
  G4UImanager::ApplyCommand
  main
```

The faulting instruction is `ldrsb x8, [x26, #0x17]` — reading the libc++
`std::string::__is_long()` flag inside the MPT's `G4MaterialConstPropertyName`
member vector. The MPT pointer itself (`0x818400007`) is wildly invalid;
some material in `G4Material::theMaterialTable` has had its MPT clobbered
between geometry construction and `BuildPhysicsTable`.

### Why this is a Heisenbug

- Reproduces deterministically under bare Release.
- **Does not** reproduce under any of:
  - Release + lldb (5/5 clean)
  - Debug + ASan (`-fsanitize=address -O1`, exit 0)
  - Release + ASan (`-fsanitize=address -O3`, exit 0)
  - Bare Release with `MallocScribble=1 MallocPreScribble=1 MallocGuardEdges=1`
- After the `kRayleigh + kMieHG` deactivation mitigation, crashes intermittently
  (~50% from 10-run trials).

That last point is the diagnostic clincher: deactivating the two processes
whose MPT walks were crashing does not eliminate the crash, it just shifts
it to a different victim process (`G4OpAbsorption`, `G4Scintillation`, etc.)
some fraction of the time. The corruption is real; we're only suppressing
its observable manifestations.

### Required trigger conditions

| `/tracking/storeTrajectory` | `--noOpticalPhotonTracking` | `sipm_model` | bare Release result |
|---|---|---|---|
| absent | absent | `"hamamatsu-s12573-100c"` | OK |
| absent | set | `"hamamatsu-s12573-100c"` | OK |
| absent | absent | `""` (GODDESS-PD) | OK |
| `1` | absent | `""` (GODDESS-PD) | OK |
| `1` | absent | `"hamamatsu-s12573-100c"` | **SIGSEGV** |
| `1` | set | `"hamamatsu-s12573-100c"` | **SIGSEGV** |

`storeTrajectory + g4sipm` is the necessary intersection. Photon tracking
is not required.

### What we know about the corrupting write

- ASan-instrumented user code + Geant4-as-stock-dylib does **not** flag any
  bad write. The G4Fibre container-overflow read we already fixed (see below)
  was unrelated; ASan with that fix in place is clean.
- The previous session's hypothesis was *"an MPT pointer in
  `theMaterialTable` is clobbered between geometry construction and
  `BuildPhysicsTable`."* The May 2026 follow-up below **disproves** that —
  pointers are stable; the bytes pointed to are not.

### May 2026 follow-up — victim identified

Instrumentation added in `g4scintkit/include/Preparation/MaterialTableSnapshot.hh`:
- `DumpMaterialTableMPTs(tag)` — called at end of `Construct()`, start
  and end of `ConstructProcess()`, start and end of `SetCuts()`. Writes
  `(material*, mpt*, name)` per material to a file. Gated by env
  `G4SCINTKIT_MPT_SNAPSHOT_PATH`. (`G4SCINTKIT_MPT_SNAPSHOT_BYTES=1`
  also dumps the first 192 bytes of each MPT, but enabling it perturbs
  heap layout enough to mask bug B — useful for healthy-run inspection
  only.)
- `InstallCrashDumper()` — async-signal-safe SIGSEGV/SIGBUS handler
  installed from `main`. On fault, dumps the faulting address and
  per-MPT bytes for every material.

Two reproducer runs of the deterministic case (`storeTrajectory 1` +
`sipm_model="hamamatsu-s12573-100c"`):

1. **All 5 pointer snapshots are byte-identical.** BisphenolA's MPT
   pointer is `0x914b475c0` from `after Construct()` through
   `leaving SetCuts()`. No MPT pointer in `theMaterialTable` is ever
   reassigned to a wild value at any of the snapshot points.
2. **At crash time, the bytes at `0x914b475c0` are a `G4SipmHousing`
   object, not an MPT.** Concretely:
   - Byte 0: a vptr in normal heap range.
   - Bytes 8-20: ASCII `"g4sipmHousing\0"` — short-string-optimised
     name set by `G4SipmHousing::G4SipmHousing` (see
     `g4sipm/g4sipm/src/housing/G4SipmHousing.cc:22`,
     `setName("g4sipmHousing")`).
   - Bytes 0x40-0x77: a sipm pointer plus seven `double`s matching the
     housing's window/package dimensions for a 6×6 mm Hamamatsu
     S12573-100C (`0x4018000000000000 = 6.0`, `0x3fc9999999999999 = 0.2`,
     etc.).
3. The faulting address loaded by `ConstPropertyExists` was
   `0x97fffea49100041d` — junk read from the now-bogus
   `G4MaterialConstPropertyName` member vector inside the overlaid object.

So the bug is: **BisphenolA's MPT heap slot has been overlaid by a
later allocation at the same address.** The MPT pointer in
`theMaterialTable` was never reassigned; the slot beneath it was.

### Root cause (confirmed by snapshot-byte bisection)

Per-material byte snapshots taken at:
1. inside `getEpoxy`, after `new G4MaterialPropertiesTable()` returns;
2. inside `getEpoxy`, after `mpt->AddProperty("RINDEX", ...)`;
3. inside `getEpoxy`, after `epoxy->SetMaterialPropertiesTable(mpt)`;
4. after `PlaceManifest` returns;
5. plus the original 5 around `Construct/ConstructProcess/SetCuts`.

show BisphenolA's MPT bytes are **valid** through (3) (vptr is the
G4MaterialPropertiesTable vtable; layout is the expected
maps + name vectors) and **already overlaid** by (4). In one run the
overlay was a `G4SipmHousing` (ASCII `"g4sipmHousing"` visible at
offset 8); in another run, with slight heap-layout drift, the overlay
was a `G4LogicalVolume` named `"containerLv"` (the g4sipm housing's
container LV). Both objects come from g4sipm's housing build, so the
allocator is consistently handing the freed BisphenolA-MPT slot to
*something* in that build path.

The freeing call chain:

- `g4sipm/g4sipm/src/MaterialFactory.cc:82-99` — `getEpoxy()` allocates
  the BisphenolA MPT and calls `epoxy->SetMaterialPropertiesTable(mpt)`.
- `g4sipm/g4sipm/src/model/impl/HamamatsuS12573100C.cc:148-156` (also
  `HamamatsuS12573100X.cc:148-156` and `G4SipmConfigFileModel.cc:124+`):
  ```cpp
  windowMaterial = new G4Material(
      name, density, MaterialFactory::getInstance()->getEpoxy());
  // ... build energies/indices ...
  G4MaterialPropertiesTable* mpt = new G4MaterialPropertiesTable();
  mpt->AddProperty("RINDEX", energies, indices, 2);
  windowMaterial->SetMaterialPropertiesTable(mpt);
  ```
- Geant4 10.6 — `G4Material::G4Material(name, density, baseMaterial, ...)`
  (`G4Material.cc:187-218`) calls `CopyPointersOfBaseMaterial()`, which
  at line 362 does
  ```cpp
  fMaterialPropertiesTable = fBaseMaterial->GetMaterialPropertiesTable();
  ```
  i.e. **shares** BisphenolA's MPT pointer into the new windowMaterial.
- Geant4 10.6 — `G4Material::SetMaterialPropertiesTable`
  (`G4Material.cc:802-816`) does
  ```cpp
  if(anMPT && fMaterialPropertiesTable != anMPT) {
    delete fMaterialPropertiesTable;     // deletes BisphenolA's MPT
    fMaterialPropertiesTable = anMPT;
  }
  ```
  Because windowMaterial inherited BisphenolA's MPT pointer one line
  before, the `delete` here frees the MPT that BisphenolA still references.

After that, BisphenolA's pointer in `theMaterialTable` is dangling; the
slot is reused by some g4sipm housing allocation; `BuildPhysicsTable`
crashes when an optical process iterates the table.

`/tracking/storeTrajectory 1` matters only because it perturbs
construction-time heap layout: without it, the allocator hands the freed
slot to something benign; with it, the slot lands inside a
`G4SipmHousing` / `G4LogicalVolume` allocation.

### Fix (applied)

Added a helper `MaterialFactory::makeEpoxyComposition(name, density)` in
`g4sipm/g4sipm/src/MaterialFactory.cc` + `MaterialFactory.hh` that builds
a fresh G4Material with epoxy's element composition WITHOUT using the
derived-material ctor, so the result starts with `fMaterialPropertiesTable
= nullptr` and the caller can safely attach its own MPT.

Replaced the three call sites that were creating windowMaterials via
the derived ctor:

- `g4sipm/g4sipm/src/model/impl/HamamatsuS12573100C.cc` (line ~148)
- `g4sipm/g4sipm/src/model/impl/HamamatsuS12573100X.cc` (line ~151)
- `g4sipm/g4sipm/src/model/impl/G4SipmConfigFileModel.cc` (line ~123)

with `MaterialFactory::getInstance()->makeEpoxyComposition(name, density)`.

Verification: with `storeTrajectory 1` + `sipm_model="hamamatsu-s12573-100c"`,
the prior 100%-crash reproducer now exits 0 on 5/5 runs. With photon
tracking enabled (drop `--noOpticalPhotonTracking`), the previously-gated
bug C (`GroupIException` at EndOfRunAction) also no longer reproduces:
3/3 clean exits.

### Fix options considered

The Geant4 behaviour is unsafe but hard to change locally. The g4sipm
code is the actionable side. Options, cheapest first:

1. **Stop attaching an MPT to BisphenolA itself** in
   `MaterialFactory::getEpoxy`. Move the RINDEX MPT to whichever places
   use epoxy directly as a logical-volume material (`G4SipmModel::getWindowMaterial`'s
   default path returns `getEpoxy()` — that consumer needs RINDEX).
   This is minimal-surface but changes a singleton's externally visible
   state.
2. **Stop using derived-material construction for windowMaterials** in
   `HamamatsuS12573100C`/`HamamatsuS12573100X`/`G4SipmConfigFileModel`.
   Construct each windowMaterial with its own composition (copy the
   epoxy element list explicitly) so it does not inherit BisphenolA's
   MPT pointer. More duplication; more local.
3. **Patch Geant4** so `SetMaterialPropertiesTable` does not delete an
   inherited (base-material-shared) MPT. Conceptually correct but
   touches an installed dependency.

(1) is the most surgical user-side fix. (2) is more invasive but
leaves both BisphenolA-as-direct-material and BisphenolA-as-base-material
working. (3) is the principled fix but requires building Geant4 with a
patch and tracking that fork.

### Related observations

The crash address fingerprint (`0x818400007`, `0x97fffea49100041d`,
etc. across runs) is the value of a clobbered `G4String::__is_long()`
byte read from an element of the overlaid object that happens to land
at the offset of `G4MaterialConstPropertyName`'s vector buffer — i.e.
the wild value is *content* of the overlay, not an MPT pointer. The
previous session's framing of *"the MPT pointer itself is wildly
invalid"* was a misread of which load was faulting.

### Previous session's residual hypotheses (now reframed)

The pre-existing patch on `g4sipm/g4sipm/src/MaterialFactory.cc` for
`getBoronCarbideCeramic()`/`getCopper()` (attaches empty MPTs) is still
benign under 10.6 (see Geant4 source analysis below) — but it lives in
the same file as the suspect `getEpoxy` allocation, and the comment
trail there is worth reading when tracking down the freed MPT.

### Proposed (not applied) mitigation: deactivate kRayleigh + kMieHG globally

Would patch all four GODDESS optical-physics .icc files
(`G4Fibre`, `G4PhotonDetector`, `OpticalCoupling`, `G4ScintillatorTile`).

Why these two, and only these two:

- `G4OpRayleigh::CalculateRayleighMeanFreePaths` was the directly observed
  crash site. It fires only in materials whose MPT sets `ISOTHERMAL_COMPRESSIBILITY`
  or that are named `"Water"`. **No material in the goddess geometry sets
  this property** — verified via `grep -r ISOTHERMAL goddess-package/`.
  Rayleigh contributes zero physics; deactivating it is free.
- `G4OpMieHG` fires only in materials whose MPT sets `MIEHG`. **No material in the
  geometry sets this property** either. Free deactivation.
- `Cerenkov`, `Scintillation`, `OpAbsorption`, `OpBoundaryProcess`, `OpWLS`
  all contribute real physics in the plastic scintillator and/or WLS fiber.
  Cannot be deactivated without losing detector response.

The deactivation site (per `.icc`):

```cpp
G4OpticalPhysics* opticalPhysics = new G4OpticalPhysics(verbose);
opticalPhysics->SetWLSTimeProfile("exponential");
opticalPhysics->SetFiniteRiseTime(true);
opticalPhysics->Configure(kRayleigh, false);   // workaround for bug B
opticalPhysics->Configure(kMieHG,    false);   // workaround for bug B
registerPhysics(physicsList, opticalPhysics);
```

This:
- 100% fixes bug C (HDF5 GroupIException at end-of-run) — exit 0.
- Cuts bug B's crash rate from 100% to ~50% in `storeTrajectory + g4sipm`
  combos. Does **not** eliminate it.

### Why the mitigation is on the shelf

Because it only halves bug B's crash rate, Kiara opted to leave it out of
the codebase for now and pursue a real root-cause fix later. A 50% crash
rate is not shippable, and committing the partial mitigation would risk
masking the deeper bug (next investigator sees ~50% crashes and thinks
they're hunting a different problem). The deactivation snippet is preserved
above so a future session can re-apply it verbatim if useful.

### What's still required to fix bug B properly

1. Locate the source of the corrupting write. Options in increasing cost:
   a. Audit goddess `G4Fibre.cc` for OOB read/writes — short list of `solids`
      and `GrandMotherAndAuntVolumes` accesses; we fixed one such read at
      line 424, others may exist.
   b. Rebuild Geant4 with `-fsanitize=address` so ASan can see writes
      inside the optical .dylibs.
   c. Try on Linux under valgrind, which doesn't perturb allocation as
      heavily as macOS ASan.
2. If user-side root cause stays elusive, consider **side-stepping at the
   launcher** — in `bash_scripts/run_visu.sh` (or the Julia wrapper), when
   `sipm_model != ""`, skip `/tracking/storeTrajectory 1` in `vis.mac`.
   Visu still works, just without trajectory rendering for g4sipm runs.

### Known related code

- `g4sipm/g4sipm/src/MaterialFactory.cc` — patched to attach empty MPTs
  to `getBoronCarbideCeramic()` and `getCopper()`. Under Geant4 10.6's
  null-safe `GetProperty(int)`, the patch is benign; it does no harm and
  is kept for symmetry with the upstream README guidance, but it was not
  load-bearing for any of the bugs we identified.
- `g4sipm/sample/sample.cc:67-74` — upstream g4sipm sample globally
  deactivates **all** five optical processes (Cerenkov, Scintillation,
  Rayleigh, MieHG, WLS). They get away with this because their geometry
  is air + SiPM only; they need no optical physics. We can't follow them
  fully because we need Cerenkov/Scintillation/WLS in the scintillator
  and fiber.

---

## G4Fibre OOB (separable bug found during the hunt)

### Symptom (ASan)

```
==ERROR: AddressSanitizer: container-overflow on address 0x602000400b90
READ of size 8 at 0x602000400b90 thread T0
    #0 G4Fibre::ConstructFibreLayerLogical G4Fibre.cc:424
    ...
0x602000400b90 is located 0 bytes inside of 8-byte region [0x602000400b90,0x602000400b98)
allocated by:
    G4Fibre::findGrandMotherAndAuntVolumes G4Fibre.cc:1241
```

### Mechanism

`G4Fibre::ConstructVolumes` (line 94) calls
`findGrandMotherAndAuntVolumes` to populate the member vector
`GrandMotherAndAuntVolumes`. For a fibre whose mother volume has no
mother-logical (i.e. mother is the world), the function pushes a single
`nullptr` and returns (`G4Fibre.cc:1241`). Vector ends with size=1, [0]=nullptr.

`ConstructFibreLayerLogical` then reads `GrandMotherAndAuntVolumes[0]`
at line 424 to gate the "outside-mother" carving. For the **first** layer
of the fibre this read is in-bounds (size=1). At line 520 there's an
unconditional `GrandMotherAndAuntVolumes.clear()` in the `else` branch
("not OnlyInsideMother and [0] is null"). After clear, `size==0` but
`capacity==1` and the buffer still contains the originally-pushed nullptr.

The **next layer** of the same fibre re-enters `ConstructFibreLayerLogical`,
re-reads `GrandMotherAndAuntVolumes[0]` — now **past `size()`** but inside
the still-allocated capacity. libc++ ASan flags this as `container-overflow`.
In bare runs the stale slot value is usually still 0 (benign), but if the
heap state caused libc++ to reuse the buffer for something else, that
something else's bytes are read and dereferenced as a `G4VPhysicalVolume*`.

### Fix

`goddess-package/source/G4BasicObjects/G4Fibre/src/G4Fibre.cc:424` — guard
the read with `!empty()`:

```cpp
if (!OnlyInsideMother && !GrandMotherAndAuntVolumes.empty() && GrandMotherAndAuntVolumes[0])
```

Verified with ASan: post-fix ASan run is clean. Does **not** fix bug B in
bare Release — the corruption that drives bug B comes from a different,
still-unknown source.

---

## Geant4 source-level facts referenced

`G4MaterialPropertiesTable` in Geant4 10.6 stores properties in:

```cpp
std::map<G4int, G4MaterialPropertyVector*> MP;
```

and `GetProperty(G4int idx, G4bool warning)` is

```cpp
auto i = MP.find(index);
if (i != MP.end()) return i->second;
return nullptr;
```

So an MPT with size-zero name vector or no matching key returns `nullptr`
safely — **not** an out-of-bounds vector access as the previous-session
hypothesis suggested. The original Cerenkov segfault hypothesis ("integer-indexed
registry desync") doesn't match the actual 10.6 implementation; whatever
that original crash was, it was either bug A (mis-reported as a
`BuildPhysicsTable` crash) or bug B (Heisenbug, with output buffering
hiding the real point of failure).

`G4Cerenkov::BuildThePhysicsTable` and `G4OpRayleigh::BuildPhysicsTable`
both walk `G4Material::GetMaterialTable()` and null-check both the MPT and
the property vector before dereferencing — they cannot segfault on a
benignly-empty MPT. They can and do segfault when the MPT pointer itself
is corrupted (bug B).

---

## Files touched in this session

In G4ScintKit (this repo):
- `g4scintkit/include/Preparation/DetectorConstruction.hh` — bug A fix
- `g4scintkit/src/Preparation/DetectorConstruction.cc` — bug A fix

In goddess-package submodule:
- `source/G4BasicObjects/G4Fibre/src/G4Fibre.cc` — G4Fibre OOB fix

In g4sipm submodule:
- `g4sipm/src/MaterialFactory.cc` — pre-existing empty-MPT patch (benign,
  kept for symmetry).
