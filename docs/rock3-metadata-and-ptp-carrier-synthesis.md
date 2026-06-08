# Rock 3 Metadata Audit And Fresh PTP Carrier Synthesis

Date: 2026-06-08

Purpose: use local educational metadata recovery plus public creator comments to understand Rock 3's structural architecture, then reason from first principles about what the PTP/Cortial rules are trying to achieve.

This document does not copy function bodies, source code, shader code, assets, or tuned constants.

## Metadata Recovery Scope

Local files inspected:

- `Rock3_Data/il2cpp_data/Metadata/global-metadata.dat`
- `Rock3_Data/ScriptingAssemblies.json`
- `Rock3_Data/RuntimeInitializeOnLoads.json`
- Local OneJS UI bundle/source-map labels already summarized in `docs/rock3-clean-room-study.md`

Recovered IL2CPP metadata:

- Metadata magic: `0xfab11baf`
- Metadata version: `31`
- Recoverable tables include string, method, field, type-definition, image, and assembly regions.
- First-party type names, field names, and method names are recoverable from metadata.
- Method bodies/native implementation were not copied into this repository.

## Recovered First-Party Structural Types

The first-party game-level types visible in metadata are:

- `Atmosphere`
- `CameraControls`
- `Climate`
- `Compute`
- `CursorDebug`
- `DataBuffers`
- `Erosion`
- `Export`
- `Icosphere`
- `OrbitMechanics`
- `SurfaceCell`
- `planetData`
- `Planet`
- `SteamManager`
- `Tectonics`
- `Tracers`

The important architecture is not hidden in a giant generic Unity object. It is explicitly organized around:

- spherical grid construction
- GPU compute dispatch
- global data buffers
- tectonics simulation
- erosion simulation
- climate simulation
- export/render surface data

## Recovered Data Model

### `DataBuffers`

Fields:

- `sim_faces_buffer`
- `sim_neighbours_buffer`
- `render_indices_buffer`
- `render_faces_buffer`
- `render_neighbours_buffer`
- `plate_arena_buffer`
- `plates_buffer`
- `mantle_flow_buffer`
- `mantle_texture`
- `crust_read_buffer`
- `crust_write_buffer`
- `arena_a_texture`
- `arena_b_texture`
- `flux_buffer`
- `surface_texture`
- `erosion_texture`
- `sdf_texture`
- `solstice_n_texture`
- `solstice_s_texture`
- `equinox_texture`
- `longitude_temps_buffer`
- `itcz_latitudes_buffer`
- `export_lut_texture`
- `export_texture`
- `surface_cell_buffer`
- `sim`
- `render`
- `max_plates`
- `max_itzc`
- `_quality`
- `_maxQuality`

Methods include:

- `CreateBuffer`
- `CreateTexture`
- `Footprint`
- `GetGPUPerformanceTier`
- accessors for sim/render dimensions, cells, vertices, plate count, and each major buffer/texture

Interpretation:

- Rock 3's core model is GPU-buffer centered.
- There are separate sim and render meshes/buffers.
- Faces and nearest-neighbor topology are explicit buffers.
- Plate state, mantle flow, crust, flux, surface, erosion, SDF, climate-season textures, and export textures are all first-class data products.
- `crust_read_buffer` and `crust_write_buffer` imply ping-pong or double-buffered crust state.
- `arena_a_texture`, `arena_b_texture`, and `plate_arena_buffer` are strong hints that plate assignment/influence is solved as a field/arena problem, not just a static plate-id map.

### `Icosphere`

Fields:

- `compute`
- `data`

Methods:

- `Create`
- `BuildGrid`
- `Dispatch`
- `Mesh`
- `Incoming`
- `Faces`
- `NearestNeighbours`

Interpretation:

- The substrate is an icosphere/spherical cell topology.
- Faces and nearest-neighbor connectivity are generated and uploaded for GPU use.
- This agrees with the creator's public statement about an icosahedral grid and nearest-neighbor/cell-polygon buffers.

### `Tectonics`

Fields:

- `compute`
- `ma_step`
- `target_ma`
- `_ma`
- `OnAgeChanged`
- `data`
- `cts`

Methods:

- `DispatchGrid`
- `DispatchCrust`
- `DispatchSurface`
- `Initialize`
- `PlateSDF`
- `SuperContinent`
- `Convect`
- `Orogenate`
- `Advance`
- `Collide`
- `Advect`
- `Merge`
- `UpdatePlates`
- `RenderSurface`
- `AdvanceTime`
- `Reset`
- `Simulate`
- `Dispose`
- `MaxTimeStep`
- `JumpFloodSteps`

Interpretation:

- Tectonics is a staged GPU pipeline.
- The stages correspond closely to the public algorithm:
  - `Initialize`: seed/initial state
  - `PlateSDF`: plate influence/distance field
  - `SuperContinent`: center-of-mass or breakup behavior
  - `Convect`: mantle/convection forcing
  - `Orogenate`: mountain-building
  - `Advance`: time stepping
  - `Collide`: local interaction/collision handling
  - `Advect`: move crust/material through the field
  - `Merge`: resolve overlapping/stacked material states
  - `UpdatePlates`: aggregate/update plate-level state
  - `RenderSurface`: derive surface fields for visualization/export
  - `JumpFloodSteps`: distance-field/SDF propagation
- The pipeline is not shaped like a Cortial remesh loop. It is shaped like a GPU cellular/particle simulation with plate-level aggregation.

### `SurfaceCell`

Fields:

- `density`
- `elevation`
- `temp_max`
- `temp_min`
- `rain_max`
- `rain_min`
- `biome`

Interpretation:

- Surface cell output is a compact diagnostic/render cell.
- It stores the exact probe values the UI displays: density, elevation, temperature range, rainfall range, and biome.
- Surface cells appear to be downstream output, not necessarily the tectonic authority itself.

### `planetData` / `Planet`

User-facing parameters:

- seed
- gravity
- axial tilt
- Milankovitch
- rotation direction
- target geological age
- plates
- hotspots
- land
- rigidity
- orogeny
- erosion
- target erosion age
- hydraulic
- thermal
- tidal
- rain
- solar constant
- wind turbulence
- greenhouse
- time of day
- projection and render toggles
- visualization mode

Interpretation:

- Rock 3 exposes a high-level planet generator surface, but the model underneath is split into tectonics, erosion, climate, atmosphere, and export modules.
- Tectonic age and erosion age are separate simulation targets.

### `Erosion`

Methods:

- `Initialize`
- `Dissolve`
- `Erode`
- `AdvanceTime`
- `Reset`
- `Simulate`
- `MaxTimeStep`

Interpretation:

- Erosion is a separate staged simulation, not merely a postprocess shader.
- `Dissolve` plus `Erode` suggests sediment/material transfer as an active state update.

### `Climate`

Methods:

- `SeedSDF`
- `CoastSDF`
- `MountainSDF`
- `SmoothHeadings`
- `ITCZ`
- `Insolate`
- `Convect`
- `FetchSurface`
- `Simulate`
- `JumpFloodSteps`

Interpretation:

- Climate derives from land/coast/mountain distance fields and orbital/solar inputs.
- This reinforces a broader design lesson: distance fields are central observability/process tools.

## Structural Conclusion About Rock 3

Rock 3 appears to be built around:

1. An icosphere/spherical grid substrate.
2. Explicit face and nearest-neighbor buffers.
3. GPU compute kernels for tectonics, erosion, climate, export, and rendering.
4. Double-buffered crust state.
5. Plate buffers and plate arena/influence buffers.
6. Mantle flow and flux fields.
7. SDF/jump-flood stages for plate, coast, mountain, and climate-distance reasoning.
8. A staged tectonics pipeline: initialize, plate SDF, supercontinent, convection, orogeny, advance, collide, advect, merge, update plates, render surface.
9. Surface cells as compact readout/export products.

This confirms that Rock 3 is not just a pretty projection over fixed ownership. It has a real moving material/process model.

## What This Teaches About PTP/Cortial

Fresh reading: PTP/Cortial's rules are not primarily about one sacred implementation shape. They protect several deeper invariants.

### Rule 1: Material Must Be Transported

PTP solution:

- Plate-local triangulations move.
- Crust values are evaluated on moving plate-local geometry.

Rock 3 solution:

- Crust state lives in moving/updateable crust buffers/particles/cells.
- Advect/collide/merge/update stages transport and reshape material.

First-principles rule:

- Tectonic truth must live on transported material, not on a fixed display grid.

### Rule 2: Projection Is A Read, Not Authority

PTP solution:

- Global TDS samples are initialization/resampling/render targets.
- Resampling reads from current plate-local geometry.

Rock 3 solution:

- `surface_cell_buffer`, `surface_texture`, and export textures appear downstream of tectonics.
- `RenderSurface` produces UI/export fields from simulation state.

First-principles rule:

- Output maps can be diagnostics and render products, but must not become the hidden source of tectonic truth.

### Rule 3: New Crust Must Be A Named Process

PTP solution:

- Divergent gaps are filled through q1/q2/qGamma oceanic-crust generation.

Rock 3 solution:

- Public comments describe empty-cell spawning at divergent boundaries.
- Metadata stages include `PlateSDF`, `Advect`, `Merge`, and `UpdatePlates`, consistent with named gap/new-material behavior.

First-principles rule:

- Missing coverage is not repaired by ownership fallback. It creates new crust through an explicit physical process.

### Rule 4: Multi-Hit/Overlap Must Be Process-State, Not Arbitrary Tie-Break

PTP solution:

- Subducting/colliding triangles are filtered before remesh sampling.
- The convergence process decides what survives.

Rock 3 solution:

- Public comments describe multiple crust particles per cell, local collisions, and merge behavior.
- Metadata includes `Collide` and `Merge`.

First-principles rule:

- Overlap is an event to classify and process. It is not just a sampling nuisance.

### Rule 5: Plate Is A Coalition, Not A Pixel Owner

PTP solution:

- A plate is a moving local triangulation with material state.
- Remesh may rebuild topology while preserving motion/state.

Rock 3 solution:

- Public comments say particles can switch plates when surrounded.
- Consumed plates can be recycled by splitting the largest plate.
- Metadata exposes plate buffers, plate arena buffers, and update stages.

First-principles rule:

- Plate identity is a dynamic material grouping. It does not have to be a permanent global sample owner.

### Rule 6: Boundaries Need Their Own Observability

PTP solution:

- Convergence, divergence, subduction, and collision state drive remeshing and terrain processes.

Rock 3 solution:

- Plate SDF, mantle flow, flux, crust buffers, and surface readouts expose a boundary/process-field architecture.

First-principles rule:

- Boundary type, boundary distance, process intensity, and material provenance should be first-class diagnostics.

## A Fresh Original Carrier Model

If we disregard our current implementation and design from first principles, the clean target is:

> A transported-material carrier over a spherical substrate, where plates are dynamic material coalitions and all projection/export surfaces are downstream reads.

### Core Substrate

- A fixed spherical sampling substrate for queries, diagnostics, and remeshing.
- The substrate may be Fibonacci+Delaunay, icosphere, geodesic grid, or another spherical topology.
- The substrate must expose nearest-neighbor/faces/area weights.

### Material Authority

Use explicit crust material records:

- material id
- position on sphere
- plate id
- density
- thickness or volume
- age
- temperature
- hydration or composition marker
- continental/oceanic material class
- elevation/isostasy state
- provenance: initial, advected, divergent-born, collision-uplifted, subducted, merged

The material record is the authority. Global render samples are not.

### Plate Authority

Use plate records as dynamic coalitions:

- plate id
- active/inactive state
- member material set or index range
- aggregate center of mass
- aggregate area/mass
- angular velocity or force accumulator
- rigidity/deformability
- boundary inventory
- lifecycle state: stable, splitting, consumed, recycled

Plate id is a grouping of material, not immutable ownership of fixed global samples.

### Motion Layer

Support two independent motion modes:

- PTP mode: analytic/geodetic plate motion for paper-faithful reproduction.
- Emergent mode: force-driven motion from local boundary/collision/mantle/center-of-mass fields.

Both modes must feed the same transported-material carrier.

### Boundary Layer

For every neighboring plate/material contact, classify:

- divergent
- convergent-subduction
- convergent-collision
- transform-like/sliding
- overlap/stacked material
- gap/uncovered surface

Each boundary event produces a named material operation:

- create oceanic crust
- subduct/destroy crust
- merge material
- thicken/uplift material
- split plate
- transfer particle/material membership

### Projection/Resampling Layer

Projection is a read from material authority:

- For a render/global sample, query current material carriers.
- If zero-hit/gap, call divergent-new-crust logic only if boundary state says that is legal.
- If multi-hit/overlap, call process-state logic, not arbitrary ownership fallback.
- If resampling, generate a new representation from current material authority and carry provenance forward.

### Invariants

The model succeeds only if:

- no material disappears except through named subduction/destruction
- no continental material appears except through named creation/uplift/collision policy
- no global sample id silently owns tectonic truth across motion
- every gap is classified before filling
- every overlap is classified before resolving
- plate aggregate state is independently recomputable from material state
- exported maps are reproducible from authority state

## Design Implication

Rock 3 and PTP solve the same deep problem with different engineering choices:

- PTP uses moving plate-local triangulations and barycentric transfer.
- Rock 3 uses GPU cell/particle buffers and process-field updates.

The common principle is stronger than either representation:

> The planet is a transported material system. Plates are carriers/groupings. Render maps are observations.

That is the design center we should use going forward.
