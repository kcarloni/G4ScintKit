# The geometry manifest format

A **manifest** is the geometry input to `g4scint`. It is a flat text file, one
placement per line, and it is the *only* way to define geometry — there are no
compiled-in setups, and `DetectorConstruction` refuses to run without
`--manifest`.

This document is for reading and hand-writing manifests directly, with no Julia
in the loop. For the *geometric* rules governing what constitutes a valid
placement — wrapping cut-lists, fibre routing, coupling geometry — see
[`placement_rules.md`](placement_rules.md); those rules are described from the
Julia builder's point of view but apply verbatim at the format level.

Working files to copy from live in `g4scintkit/examples/`; the top-level
[README](../README.md) describes each and how to run it.

## Shape of the file

```
# comments begin with '#' and are ignored, as are blank lines
SETUP  <label>
SCINT  name=… dims=… pos=… …
FIBER  name=… mother=… start=… end=… …
WRAP   scint=… material=… …
SIPM   name=… ref_volume=… …
CASING module_half_x=… …
```

Every line is `TYPE key=value key=value …`. There is no nesting and no quoting:
**values may not contain spaces**, and vectors are written as comma-separated
components (`pos=0,0,0`, `rot=1,0,0,0,1,0,0,0,1` row-major). All quantities are
in Geant4 internal units — **millimetres and radians**.

**File order is construction order**, and this is geometry-critical, not
cosmetic: GODDESS's `ConstructWrapping` subtracts every volume that exists at
the moment it is called, so a `WRAP` line placed before its bar's fibres
produces different — usually wrong — geometry than one placed after. Write
`SCINT`, then that bar's `FIBER`s, then its `WRAP`, then `SIPM`.

## Entry types

`SETUP` takes a single bare label (not `key=value`) and is optional; it only
names the geometry in the output.

### `SCINT` — a scintillator tile

| Key | Meaning |
|---|---|
| `name` | Manifest-local handle; other lines refer to the volume by this |
| `g4name` | Geant4 volume name. Empty = leave GODDESS's default |
| `dims` | **Full** extents (x,y,z), not half-lengths |
| `pos` | Translation within `mother` |
| `rot` | Row-major 3×3 rotation matrix, 9 components |
| `mother` | Enclosing volume's `name`, or `world` |
| `material` | GODDESS `.properties` file — see below |
| `sensitive` | `1` to attach a sensitive detector, `0` for passive |

### `FIBER` — one fibre segment

A routed fibre is many `FIBER` lines sharing a `loop_id`, each `straight` or
`bent`.

| Key | Meaning |
|---|---|
| `name`, `mother` | As above |
| `kind` | `straight` or `bent` |
| `start`, `end` | Endpoints, in `mother`'s frame |
| `bend_angle`, `bend_axis` | Radians and axis; both zero for `straight` |
| `material`, `glue_file` | `.properties` files; `glue_file` may be empty |
| `glued`, `glue_profile` | `1`/`0`, and `round` or `square` |
| `reference` | Optional reference volume name |
| `start_refl`, `end_refl` | End reflectivities. `nan` = don't call the setter. `start_refl` may be omitted entirely |
| `loop_id` | Groups segments of one logical fibre; `-1` = ungrouped |

### `WRAP` — reflective wrapping on a tile

| Key | Meaning |
|---|---|
| `scint` | `name` of the `SCINT` to wrap |
| `g4name` | Geant4 volume name, may be empty |
| `material` | `.properties` file |
| `cut` | Comma-separated volume names to exclude, may be empty |

### `SIPM` — a photon detector

| Key | Meaning |
|---|---|
| `name`, `ref_volume` | Handle, and the volume its position is relative to |
| `face_dir` | Outward normal of the sensitive face |
| `rel_pos` | Position within `ref_volume` |
| `edge_length` | Square sensor edge, mm |
| `fiber` | `name` of the fibre segment being read out |
| `coupling_normal`, `coupling_pos`, `coupling_width` | Optical coupling slab |
| `fiber_is_base` | `1` if `coupling_pos` is in the fibre's frame |
| `model` | Optional g4sipm model id. Omit for the GODDESS photon detector |

### `CASING` — optional aluminium box and lead sheet

One line, always last. Set `aluminum_thickness=0 lead_thickness=0` to disable
it; the remaining fields are then ignored. Fields: `module_half_x`,
`module_min_y`, `module_max_y`, `module_half_z`, `aluminum_thickness`,
`lead_thickness`, `num_bars`, `bar_width`, `scinti_z`.

**`CASING` is the one entry that assumes a particular orientation.** Everywhere
else you are free — `SCINT`, `FIBER` and `SIPM` all take an arbitrary `pos` and
`rot`, so a detector may sit at any angle. The casing, however, is built
axis-aligned with a specific meaning per axis:

| Axis | Role |
|---|---|
| **y** | the thin, vertical axis. The lead sheet is a slab in the x–z plane of half-thickness `lead_thickness/2`, and `module_min_y` / `module_max_y` are a *signed* vertical extent rather than a half-width |
| **x** | the direction bars are stacked in. The lead sheet spans `num_bars * bar_width` |
| **z** | the bar long axis. The lead sheet spans `scinti_z / 2` |

So a geometry with bars long in z, thin in y and stacked in x can use `CASING`
as-is. One built in some other orientation cannot, and should disable the
casing and place its own enclosure with ordinary `SCINT` entries.

## Material paths

`material=` and `glue_file=` name GODDESS `.properties` files. They may be
either absolute, or **relative to the GODDESS package root**:

```
material=source/MaterialProperties/Scintillator/Fermilab_scintillator.properties
```

Relative is strongly preferred — it is what makes a manifest portable between
machines, so it can be committed to version control and shared. Relative paths
are resolved against the `GODDESS` environment variable, which
`bash_scripts/setup_paths.sh` exports; if it is unset you get an explicit error
naming the offending path. Absolute paths are passed through untouched, so
older manifests keep working.

Resolution happens at the point the file is opened, never at parse time, so the
values you write are the values stored: the per-run `geometry.manifest` dump in
the output directory is byte-identical to the manifest you supplied.

> Do **not** rely on paths relative to your shell's working directory.
> `RunSimulation` calls `chdir($BUILDDIR)` before constructing geometry, so
> such a path would silently resolve against the build directory.

## Minimal hand-written example

One bar, one straight fibre, one SiPM, no wrapping or casing. This exact file
is verified to build and run:

```
SETUP hand_written
SCINT name=bar g4name= dims=49.5,9.5,1875 pos=0,0,0 rot=1,0,0,0,1,0,0,0,1 mother=world material=source/MaterialProperties/Scintillator/Fermilab_scintillator.properties sensitive=1
FIBER name=fib kind=straight mother=bar start=12.375,0,-900 end=12.375,0,958.5 bend_angle=0 bend_axis=0,0,0 material=source/MaterialProperties/Fibre/Kuraray_Y11-300_round_1mm.properties reference= glued=0 glue_file= glue_profile= end_refl=nan loop_id=-1
SIPM name=sipm ref_volume=bar face_dir=0,0,-1 rel_pos=12.375,0,958.75 edge_length=3 fiber=fib coupling_normal=0,0,-1 coupling_pos=0,0,0 coupling_width=0.25 fiber_is_base=1
CASING module_half_x=0 module_min_y=0 module_max_y=0 module_half_z=0 aluminum_thickness=0 lead_thickness=0 num_bars=0 bar_width=0 scinti_z=0
```

Run it:

```bash
source bash_scripts/setup_paths.sh
bash g4scintkit/run.sh --outdir output/hand --manifest my.manifest --nevents 10
```

## Things that will catch you out

- **The world volume is a fixed 10 m cube** and is not manifest-controlled.
  Geometry larger than that will not fit.
- **A `nan` path length in the output means the beam missed.** Check
  `--injpos` against your bar positions; a gap between bars centred on the beam
  axis deposits exactly nothing.
- **`dims` is full extents**, while `module_half_x` / `module_half_z` are
  half-lengths. Mixing these up is the most common hand-writing error.
- **Reordering lines changes the geometry.** See the ordering note above.
- **The fibre must protrude past the face the SiPM reads.** If a fibre ends
  inside its mother volume and you place the SiPM beyond that end, the optical
  coupling has no volume to live in, and construction aborts with

  ```
  *** G4Exception : InvalidSetup
        issued by : G4OpticalCoupling::G4OpticalCoupling(...)
  Mother volume for coupling could not be determined.
  ```

  The message above it names the three volumes that disagree. Extend the
  fibre's `end` past the bar face and put `rel_pos` just beyond it, as the
  example above does
  (fibre ends at z = 958.5 on a bar whose face is at z = 937.5; SiPM sits at
  958.75).
