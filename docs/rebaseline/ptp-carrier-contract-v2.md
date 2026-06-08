# PTP Carrier Contract V2

Date: 2026-06-08

Purpose: define the restart contract for recreating the PTP/Cortial carrier from first principles, using the current project lessons and the Rock 3 structural comparison to clarify the underlying rules.

## Design Center

The planet is a transported material system.

Plates are carriers or coalitions of crust material. Meshes, samples, grids, textures, and maps are representations used to transport, query, remesh, diagnose, or render that material.

The paper-faithful implementation can still use plate-local duplicated triangulations. The deeper rule is that tectonic truth must live on moving material authority, not on fixed output samples.

## Source Authority

Normative sources:

- Cortial et al., "Procedural Tectonic Planets" (2019)
- Cortial thesis, "Synthese de terrain a l'echelle planetaire" (2020)
- Local extraction docs:
  - `docs/paper-carrier-extraction.md`
  - `docs/paper-resampling-extraction.md`
  - `docs/phase-iii-paper-process-extraction.md`

Rock 3 role:

- Clarifies architectural concepts such as transported crust buffers, process stages, and observability.
- Does not override the PTP/Cortial contract.
- May inspire diagnostics and design questions, not paper behavior.

## Contract Layers

### 1. Substrate Layer

The substrate supplies stable sampling/query topology:

- deterministic sphere samples
- area weights
- spherical triangulation/topology
- neighbor relationships
- projection/remesh targets

In the thesis-literal path this is the global Fibonacci TDS.

The substrate is not tectonic authority.

### 2. Material Authority Layer

Crust/material fields are the state being transported.

At minimum, each authoritative crust record must expose:

- material class: continental/oceanic/empty/unknown
- elevation or thickness proxy required by the paper slice
- age or historical field when the paper process requires it
- plate association
- provenance: initial, advected, resampled, divergent-born, subducted, collided, sutured
- validity/process flags

The representation may be plate-local vertices, plate-local samples, or another paper-equivalent carrier, but the restart begins with the thesis-literal plate-local triangulation model.

### 3. Plate Carrier Layer

Each plate owns moving material authority.

Thesis-literal plate record:

- plate id
- geodetic motion parameters
- duplicated local vertices
- duplicated/re-indexed/re-compacted local triangles
- empty neighborhoods at plate boundaries
- plate-local material fields
- boundary/process marks when those stages exist

Plate id is not a permanent global sample owner.

### 4. Motion Layer

Stage 1 motion is analytic/geodetic:

- plate-local vertices physically rotate by the plate's motion
- plate-local vector fields rotate with the plate when present
- global substrate remains fixed
- material state remains attached to the moving plate-local carrier

Motion must be independently recomputable from plate motion parameters and elapsed time.

### 5. Projection Layer

Projection is a read from current material authority.

For a global sample `p`:

- cast the center-to-`p` ray
- test against plate-local triangle meshes
- record every raw hit
- record boundary degeneracy
- apply only the process-state filters allowed for the current stage
- transfer material by barycentric interpolation from the selected valid hit

Projection output is diagnostic/render state, not authority.

### 6. Boundary Process Layer

Boundary state must exist before paper-faithful remeshing can resolve convergent overlaps.

Required process classifications:

- divergent gap
- convergent/subduction
- convergent/collision
- boundary degeneracy
- numeric miss/overlap
- topology defect

Subduction/collision marks are not optional tie-break flavor. They are the paper's answer to what survives in overlap cases.

### 7. Resampling Layer

Resampling is a named cross-window operation:

- pre-treat plates
- exclude subducting/colliding triangles where the thesis requires it
- sample current moving plate-local authority onto the global TDS
- fill divergent zero-hit samples through q1/q2/qGamma oceanic-crust generation
- rebuild plate-local duplicated triangulations
- reset/invalidate process state exactly as the thesis specifies

Resampling is not ownership repair.

### 8. Output Layer

Maps, textures, actor views, contact sheets, and exported metrics are observations.

Required outputs for gates:

- raw hit count map
- selected plate map
- miss/overlap class map
- divergent gap map
- boundary degeneracy map
- subduction/collision filtered-hit map once those processes exist
- material provenance map
- per-plate area/mass/material summary
- replay hash table

## Hard Invariants

The restart kernel must enforce:

- no persistent global sample ownership as material authority
- no hidden prior-owner fallback
- no gap fill without named divergent process classification
- no multi-hit resolution that bypasses subduction/collision process state in paper-faithful remeshing
- no material loss without a named destructive process
- no material creation without a named creation process
- no pass verdict from visuals alone
- no stage advancement without a checkpoint and explicit go/no-go

## Early Stage Acceptance

Stage 0 acceptance:

- global TDS created deterministically
- plate-local duplicated topology built from global TDS
- ray-from-origin projection works at t=0
- non-degenerate misses are zero
- non-degenerate overlaps are zero
- boundary-degenerate cases are counted and bounded
- material authority and projected output are reported separately

Stage 1 acceptance:

- plate-local vertices rotate physically
- material moves with plate-local geometry
- drift oracle is independent
- projection reads from moved authority
- same-seed replay hashes are stable
- no remeshing, no mutation, no process repair

Pre-remesh acceptance:

- convergence/subduction/collision state exists
- paper filtering rule can be exercised before candidate selection
- q1/q2 continuous boundary provenance is implemented or explicitly bounded as an approximation
- material accounting gates are in place

## Rock 3 Lesson Folded Into The Contract

Rock 3's recovered structure makes one thing vivid:

- `crust_read/write`, `plates`, `plate_arena`, `mantle_flow`, `flux`, `sdf`, and `surface_cell` separate authority/process/output concerns.

PTP needs the same conceptual separation even if the data structures differ:

- carrier authority
- process fields
- projection/output fields

The restart should name these layers explicitly in code and reports.
