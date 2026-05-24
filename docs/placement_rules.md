# Geometry manifest — placement rules

How to place the pieces of a TAMBO detector geometry in the GODDESS / manifest
system. The manifest is the flat, fully-explicit description that the C++
`DetectorConstruction::PlaceManifest` interpreter consumes; the Julia
`ManifestBuilder` (`add_*!` functions) is the front end that builds one and
validates it.

There are **four placement-entry kinds** — `SCINT`, `FIBER`, `WRAP`, `SIPM` —
plus a single `CASING`. The optical coupling is *not* its own entry: it is built
as part of a `SIPM`.

---

## 1. Universal rules (apply to everything)

**Units.** Every number is a plain `Float64` in Geant4 internal units —
**millimetres** and **radians**. The unitful, user-facing layer is `DetectorSpec`
(`strip_units` converts it).

**Coordinates are always relative to a parent.** Nothing takes a global
coordinate. Geant4's `G4PVPlacement` always places a volume in its mother's local
frame; the manifest inherits this. `"world"` is just the root of the relative
chain — coordinates are "world coordinates" only when the parent resolves to
`"world"`.

**Placement order is geometry-critical.** The order of entries in the manifest
*is* the C++ construction order. Two consequences:

- A referenced volume **must be added before** the entry that references it. A
  forward reference is rejected by `_validate_references` (it fails loudly rather
  than silently producing wrong geometry).
- The canonical per-tile order is **scintillator → its fibres → its wrapping**,
  because the wrapping cuts holes where fibres penetrate it and GODDESS wrapping
  cuts are construction-order dependent.

**Names.** `SCINT`, `FIBER`, and `SIPM` each carry a unique, non-empty `name`.
Duplicates and empties are rejected at `add` time. `WRAP` has no name of its own
(it is identified by the scintillator it wraps). Names are how later entries
cross-reference earlier ones.

**Materials.** A material is a `.properties` file path, resolved once from the
spec's abbreviations (`:scint`, `:wrap`, `:wls`, `:cement` keys in the builder).
A material path **must not contain a space** — the flat manifest format is
space-delimited and would not parse back.

**Validation on `to_manifest`.** Finalising the builder runs, in order:
1. cross-reference validation — every `mother` / `reference` / `fiber` / `scint`
   names a volume of the right kind placed *earlier*;
2. fibre–fibre clash check;
3. scintillator–scintillator overlap check.

By default a clash or overlap throws (`on_clash = :error`).

---

## 2. Scintillator (`SCINT`)

The active detector tile. **Builder:** `add_scint!`.

| Field | Rule |
|---|---|
| `name` | unique, non-empty |
| `g4name` | optional; `""` means "do not call `SetScintillatorName`" |
| `dims` | full `(x, y, z)` size in mm |
| `pos` | translation **in the `mother`'s frame** (default `(0,0,0)`) |
| `rot` | row-major 3×3 rotation matrix, **in the `mother`'s frame** |
| `mother` | `"world"` or the name of an **earlier** scintillator |
| `material` | `:scint` key or a literal `.properties` path |
| `sensitive` | `true` registers it as an active sensitive detector |

**Rules**
- The scintillator is the **only volume that can serve as a parent frame** for
  other entries — i.e. only `"world"` or a scintillator can be a `mother`, a
  fibre `reference`, or a SiPM `ref_volume`.
- A scintillator's `mother` may be another scintillator; the `mother` chain is
  resolved recursively to world coordinates.
- **No two scintillators sharing a mother may overlap.** Their oriented bounding
  boxes are tested (`scint_overlaps`); an overlap makes Geant4 navigation
  ambiguous and double-counts energy deposits.

---

## 3. Optical fibre (`FIBER`)

A WLS fibre. **Builders:** `add_fiber_straight!`, `add_fiber_bent!`,
`add_fiber_routed!`.

**One `FiberEntry` is one *segment*** — straight or a single circular arc. A
physical fibre that bends is several entries chained end to end.

| Field | Rule |
|---|---|
| `name` | unique, non-empty |
| `kind` | `"straight"` or `"bent"` |
| `start`, `stop` | endpoints, **in the `reference` frame if set, else the `mother` frame** |
| `mother` | volume the fibre is *placed into and clipped against*; `"world"` or an earlier scintillator |
| `reference` | optional; if set, an earlier scintillator — **only re-frames `start`/`stop`**, does not change placement |
| `bend_angle`, `bend_axis` | bent only — angle in radian, axis a 3-vector |
| `glued` | if `true`, an optical-cement sheath is built around it |
| `glue_profile` | cement cross-section, e.g. `"round"` |
| `end_reflectivity` | `NaN` = not set; a value sets a reflective fibre end |
| `loop_id` | logical-fibre id (see below) |

**Frame rule (the subtle one).** `start`/`stop` live in the `reference`
volume's frame when `reference` is set, otherwise in the `mother`'s frame. The
`mother` separately decides which volume the fibre is embedded in and carved
against. B2 uses `mother = "world"` with `reference = "scint_N"` so each bar's
routing is written in that bar's local frame but placed in the world.

**`loop_id` — logical fibre identity.**
- Segments sharing a `loop_id >= 0` are pieces of **one continuous physical
  fibre**. They are expected to touch, are never clash-checked against each
  other, and their lengths are summed for reporting.
- `loop_id < 0` means a stand-alone single-segment fibre; its length is reported
  individually.
- `add_fiber_routed!` auto-allocates one `loop_id` for the whole routed run.

**Ordering.** A fibre must be placed **after** its `mother`/`reference`
scintillator and **before** that scintillator's `WRAP`.

**No fibre–fibre overlap.** Two segments that share a `mother`, belong to
*different* logical fibres, and whose centrelines (lifted to world coordinates)
pass closer than `clearance` (default 1.0 mm ≈ fibre diameter) are flagged as a
clash. Fibres in *different* mothers are never compared. Self-intersection of one
logical fibre is **not** caught.

---

## 4. Reflective wrapping (`WRAP`)

A reflective coating around a scintillator tile. **Builder:** `add_wrapping!`.

| Field | Rule |
|---|---|
| `scint` | name of an **earlier** scintillator — the tile being wrapped |
| `g4name` | optional GODDESS wrapping name |
| `material` | `:wrap` key or a literal path |
| `cut` | volumes the wrapping is carved against (see below) |

**Rules**
- A `WRAP` **has no coordinates** — it conforms to the tile named by `scint`.
- It **must be placed after every fibre that penetrates that tile**, because the
  wrapping is cut where fibres pass through it. Placing it too early means the
  fibres do not yet exist to cut against.
- `cut` controls the cut-volume candidates:
  - **empty (recommended)** — `PlaceManifest` auto-derives the candidates from
    the reference graph: every fibre already placed whose `mother` *or*
    `reference` is this scintillator. GODDESS then geometrically filters those
    down to the ones that actually pierce the shell.
  - **non-empty** — a manual override; the listed volume names are used verbatim.
- One wrapping per scintillator is the normal case.

---

## 5. SiPM (`SIPM`)

The photon detector. **Builders:** `add_sipm!` (raw), `add_inline_sipm!`
(helper that derives the coupling geometry for a straight readout fibre).

A `SIPM` entry carries **two independent volume references** that do different
jobs — do not conflate them:

| Field | Role |
|---|---|
| `ref_volume` | the **placement frame** for the SiPM body |
| `fiber` | the **optical coupling partner** |

| Field | Rule |
|---|---|
| `name` | unique, non-empty |
| `ref_volume` | `"world"` or an **earlier scintillator** — *not* a fibre |
| `face_dir` | sensitive-surface normal, **relative to `ref_volume`** |
| `rel_pos` | sensitive-surface position, **relative to `ref_volume`** |
| `edge_length` | side length of the square sensitive face (mm) |
| `fiber` | name of an **earlier** `FIBER` to couple to |
| `coupling_*` | see §6 |
| `fiber_is_base` | see §6 |

**Rules**
- `ref_volume` and `fiber` are unrelated fields. `ref_volume` is purely the
  coordinate frame for `rel_pos`/`face_dir`; `fiber` is the optical target.
- `ref_volume` can be `"world"` (B2) or a scintillator (B1, B3). The builder
  **rejects a fibre** as `ref_volume`.
- `fiber` must name a fibre placed earlier in the list.
- The SiPM is normally placed **last**, after all fibres and wrappings.
- Caveat — in `g4sipm` mode the world position is computed as
  `ref_volume.GetObjectTranslation() + rel_pos`, which is a true world position
  only when `ref_volume`'s mother is the world (one translation level, rotation
  ignored). The default GODDESS path has no such limitation.

---

## 6. Optical coupling

The optical coupling slab between fibre and SiPM. **It is not a separate
manifest entry** — it is built automatically as part of the `SIPM` (GODDESS
path only).

| Field (on the `SIPM` entry) | Rule |
|---|---|
| `coupling_normal` | coupling-surface normal, **relative to the base volume** |
| `coupling_pos` | coupling-centre position, **relative to the base volume** |
| `coupling_width` | thickness of the coupling slab (mm) |
| `fiber_is_base` | selects the base volume: `true` → the fibre, `false` → the SiPM |
| `glue_material` | the coupling material (`:cement`) |

**Rules**
- `coupling_normal`/`coupling_pos` are measured in the frame of the **base
  volume**, which is the fibre when `fiber_is_base = true` and the SiPM when
  `false`. This frame is distinct from the SiPM's `ref_volume`.
- In **`g4sipm` mode** no external coupling is built — the SiPM housing's
  built-in epoxy window (n = 1.5) is the optical interface, and the housing is
  shifted toward the fibre by `coupling_width` to compensate.
- `add_inline_sipm!` packages all of this: give it the readout fibre's two
  endpoints (in the SiPM's `ref_volume` frame) and it derives `face_dir`,
  `rel_pos`, `coupling_normal`, and `coupling_pos` so the SiPM sits flush
  `coupling_width` beyond the fibre end, facing back down it.

---

## 7. Casing (`CASING`)

The outer aluminium box and lead sheet. **Builder:** `CasingSpec` directly, or
`casing_from_extent` to auto-derive it.

| Field | Rule |
|---|---|
| `module_half_x`, `module_half_z` | half-extents of the module bounding box (**world frame**) |
| `module_min_y`, `module_max_y` | y bounds of the module (**world frame**) |
| `aluminum_thickness` | `<= 0` disables the aluminium box |
| `lead_thickness` | `<= 0` disables the lead sheet |
| `num_bars`, `bar_width`, `scinti_z` | semantic fields, passed straight through |

**Rules**
- There is **exactly one** casing per manifest, and it is **always placed last**,
  directly in the world volume — after every other placement.
- `casing_from_extent` builds the bounding box from every **world-mothered**
  scintillator and fibre endpoint, and **assumes the module is centred on the
  origin in x and z**. Mother-relative placements are skipped (their coordinates
  are not in the world frame), so a deeply nested design needs the box set by
  hand.

---

## Quick reference — what frame is each coordinate in?

| Entry | Coordinate field(s) | Frame |
|---|---|---|
| `SCINT` | `pos`, `rot` | the `mother` volume |
| `FIBER` | `start`, `stop`, `bend_axis` | the `reference` volume, else the `mother` |
| `WRAP` | *(none)* | conforms to the wrapped tile |
| `SIPM` | `rel_pos`, `face_dir` | the `ref_volume` |
| `SIPM` | `coupling_pos`, `coupling_normal` | the coupling base volume (fibre or SiPM) |
| `CASING` | `module_half_*`, `module_min/max_y` | world |

## Quick reference — what can each "parent" field name?

| Field | Allowed values |
|---|---|
| `SCINT.mother` | `"world"` or an earlier scintillator |
| `FIBER.mother` | `"world"` or an earlier scintillator |
| `FIBER.reference` | `""` or an earlier scintillator |
| `WRAP.scint` | an earlier scintillator |
| `SIPM.ref_volume` | `"world"` or an earlier scintillator |
| `SIPM.fiber` | an earlier fibre |
