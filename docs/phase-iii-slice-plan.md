# Phase III Slice Plan

Status: draft for user review. Each slice ends with a checkpoint note and requires explicit go/no-go before the next slice begins. Sub-phase boundaries (IIIA, IIIB, IIIC, IIID, IIIE, IIIF, IIIG, IIIH) require their own consolidated sub-phase checkpoint in addition to per-slice checkpoints.

## Phase III Observability Patch: Read-Only Heatmaps

Goal: give Phase III spatial sanity checks before Phase IV amplification without changing geometry, authority, projection, or process behavior.

Allowed actor layers:

- `ElevationHeatmap`: colors samples by stored Phase III `Elevation`; zero remains sea-level green, negative values trend blue, positive values trend red.
- `SubductionMask`: colors existing IIIB polarity evidence from `ConvergenceSubductionTriangleHits` and polarity decisions; under-plate, over-plate, collision-candidate, and deferred evidence are visualized as separate read-only colors.
- `DistanceToFrontHeatmap`: colors active boundary-triangle vertices by their stored IIIB distance-to-front value; unset samples remain dark.

Rules:

- These layers are not gates and do not replace numerical/audit checkpoints.
- They must read existing Phase III fields only. No inferred ownership, no projection changes, no material mutation, no smoothing, no terrain displacement.
- Vertex positions remain on the unit sphere; Phase IV remains responsible for amplified terrain geometry.

## Sub-Phase IIIA: Paper Crust State Schema

Goal: add the paper's crust-state fields to the carrier vertex/sample without changing tectonic behavior. Storage and rotation only; no consumers, no mutation outside rotation.

### IIIA.1: Inert Elevation Field

Work:

- Add `Elevation` (double, km) to `FCarrierVertex` and any plate-local vertex copies.
- Initialize to `0.0` at carrier construction and resampling.
- Propagate through the per-plate triangulation construction (duplicate / re-index / re-compact) without modification.
- Add a separate `crust_state_hash` that includes `Elevation`. Phase II hashes (`projection_hash`, `state_hash`) must remain unchanged with `Elevation` zeroed.

Exit gate:

- Phase II baseline hashes match exactly: same projection, same state, same material ledger.
- New `crust_state_hash` is deterministic across same-seed replays.
- Inert Elevation field survives at least one full resampling event without value drift.

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiia1-report.md`.

### IIIA.2: Inert Oceanic Age

Work:

- Add `OceanicAge` (double, Ma) to `FCarrierVertex`.
- Initialize to `0.0` for samples with `bContinental == true`, leave `0.0` for oceanic samples (paper: oceanic age is set to 0 at ridge generation; IIIA stores it but does not yet generate at ridges — that's IIIE).
- Extend `crust_state_hash` to include `OceanicAge`.

Exit gate:

- Phase II baseline hashes unchanged.
- `OceanicAge` survives resampling unmodified at this slice.
- `crust_state_hash` is deterministic and includes the new field.

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiia2-report.md`.

### IIIA.3: Inert Vector Fields With Per-Step Rotation

Work:

- Add `RidgeDirection` (FVector3d, unit vector tangent to the sphere at the sample) and `FoldDirection` (FVector3d, same convention) to `FCarrierVertex`.
- Initialize to zero vectors.
- Rotate both vectors per timestep alongside the existing geodetic rotation of vertex positions, using the same rotation transform applied to the sample's plate. Tangency to the sphere is preserved by the rotation.
- Vectors with zero magnitude are left zero (no normalization division by zero).
- Extend `crust_state_hash` to include both vector fields.

Exit gate:

- Phase II baseline hashes unchanged.
- Zero-motion fixture: vector fields remain zero or unchanged.
- Forced-rotation fixture: a non-zero seed vector at one sample rotates per the plate's geodetic motion; analytical comparison passes within numerical tolerance.

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiia3-report.md`.

### IIIA.4: Field Interpolation Through Resampling

Work:

- Extend the resampling path to barycentric-interpolate `Elevation`, `OceanicAge`, `RidgeDirection`, and `FoldDirection` from the hit triangle to the new TDS sample.
- Vector field interpolation: barycentric-interpolate components, then re-tangent to the sphere (project out the radial component) and re-normalize if magnitude > 0.
- Resampling gap-fill samples receive zeros for new fields (oceanic-generation params are populated in IIIE, not here).

Exit gate:

- Crust fields propagate through resampling deterministically; `crust_state_hash` is replay-stable.
- Boundary smear test: a synthetic field discontinuity at a plate boundary produces measurable barycentric-interpolated values at near-boundary samples after one resample. Quantify the smear (analogous to Slice 5.5's source-uniformity classification, but for the new fields).
- All Phase II material ledger gates continue to pass.

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiia4-report.md`.

### IIIA Consolidation

Work: write `docs/checkpoints/phase-iii-iiia-consolidated.md` summarizing IIIA.1–IIIA.4 results and confirming all Phase II hashes remain intact and `crust_state_hash` is added cleanly.

## Sub-Phase IIIB: Convergence Tracking (Read-Only)

Goal: build paper-faithful convergence tracking state per thesis §3.3.1.3, without mutating crust data or filter behavior. All decisions are logged; no triangle is yet marked as subducting in a way that affects projection.

### IIIB.1: Boundary Active List Scaffold

Work:

- Add `ActiveBoundaryTriangles` per-plate persistent state (per-event-window, reset at remesh).
- Initialize from each plate's boundary triangles at the start of each remesh window.
- Add a `convergence_tracking_hash` independent of the crust state hash.

Exit gate:

- Active list is deterministic across replays.
- Active list resets at every remesh.
- No filter behavior changes.

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiib1-report.md`.

### IIIB.2: Per-Triangle Distance-To-Front

Work:

- Add `DistanceToFront` (double, km) per active triangle.
- Initialize to 0 when a triangle enters the active list (i.e., is on the convergence front).
- Update each step: `d(p, t+δt) = d(p, t) + s(p)·δt` per thesis over-estimation formula.
- Triangles with `d > r_s` (paper: 1800 km) are removed from the active list.

Exit gate:

- Distance-to-front trajectory is deterministic across replays.
- Forced-convergence fixture: distance-to-front at fixture front samples evolves per the analytical expectation.
- Active list shrinks correctly when triangles exceed `r_s`.

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiib2-report.md`.

### IIIB.3: Subduction Matrix

Work:

- Add per-event-window `SubductionMatrix` (sparse N×N plate flag matrix).
- Populate from active-triangle ray-from-origin-to-barycenter intersection tests against per-plate BVHs (consistent with thesis §3.3.1.3 detection mechanism).
- Reset at every remesh.

Exit gate:

- Matrix is deterministic across replays.
- Matrix is empty at step zero of every remesh window.
- Forced-convergence fixture populates the expected plate pair; forced-divergence and zero-motion leave the matrix empty.

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiib3-report.md`.

### IIIB.4: Oceanic-Under-Continental Polarity Rule

Work:

- For each subduction matrix entry, evaluate polarity:
  - If one plate is dominantly oceanic and the other dominantly continental: oceanic is under.
  - If both continental: classify as collision-candidate (no polarity).
  - If both oceanic: defer to IIIB.5 (age-based).
- Polarity decisions are logged read-only; no triangle mutation yet.

Exit gate:

- Polarity rule fires deterministically.
- Mixed-material fixture labels oceanic-under correctly; polarity-swap fixture flips it.
- Continental-continental fixture produces collision-candidate, not subduction polarity.

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiib4-report.md`.

### IIIB.5: Ocean-Ocean Age Polarity Rule

Work:

- For oceanic-oceanic subduction matrix entries, evaluate age polarity using `OceanicAge` (still inert from IIIA, so test fixtures provide non-zero ages directly).
- Older oceanic plate subducts under younger.
- Polarity is logged read-only.

Exit gate:

- Age polarity rule is deterministic.
- Forced-ocean-ocean-aged fixture (older plate one side, younger the other) labels the older plate as under.
- Reversing the ages reverses the polarity.

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiib5-report.md`.

### IIIB.6: Neighbor Propagation

Work:

- When a triangle becomes "actively convergent" (passes detection and polarity), add its plate-local neighbor triangles to the active list.
- Neighbor propagation is bounded by `d > r_s` (already in IIIB.2) so the active list cannot grow unboundedly.

Exit gate:

- Neighbor propagation produces the expected active-list growth pattern from a single-triangle seed under forced-convergence.
- Active list does not grow unboundedly under any tested fixture.
- Replay determinism preserved.

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiib6-report.md`.

### IIIB.7: Replay/Event Hash Closure

Work:

- Consolidate convergence tracking state into a stable hash including active triangle membership, distance-to-front per triangle, subduction matrix, and polarity decisions per matrix entry.
- Add to per-step metrics output.

Exit gate:

- All IIIB hashes deterministic across same-seed replays.
- Phase II hashes unchanged; no projection/filter/material behavior modified.

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiib7-report.md`.

### IIIB Consolidation

Work: `docs/checkpoints/phase-iii-iiib-consolidated.md` summarizing tracking-state determinism and confirming no Phase II regressions.

## Sub-Phase IIIC: Continuous Subduction / Obduction (Mutation)

Goal: paper-faithful subduction effects on plate state, including the slab-pull feedback into carrier authority. This is the first sub-phase that mutates carrier state (Elevation field) and motion (rotation axis).

### IIIC.1: Mark Subducting Triangles

Work:

- Triangles meeting subduction polarity criteria from IIIB are marked as subducting in the per-event-window state.
- Marking persists across timesteps within the window; reset at remesh.
- The remesh path's existing "exclude subducting/colliding triangles at projection time" behavior consumes this marking (currently it consumes Phase II's per-event labels; IIIC.1 makes the source persistent across multiple steps).

Exit gate:

- Subducting triangle set is deterministic across replays.
- Resampling correctly excludes triangles marked any time during the prior window.
- Phase II's per-event subduction filter behavior is unchanged when no IIIC mutations are configured.

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiic1-report.md`.

### IIIC.2: Visible vs Historical Elevation Split

Work:

- Add `HistoricalElevation` field to subducting triangle vertices on first marking. Snapshot the current `Elevation` value into `HistoricalElevation`.
- Visible `Elevation` of subducting triangles is set to a trench-style depth (paper Table 3.2 `z_t = -10 km`).
- `HistoricalElevation` is what the over-plate uplift formula will read in IIIC.3.

Exit gate:

- Snapshot is taken exactly once per triangle per remesh window.
- Visible and historical elevations are independently hashable.
- Trench depth applied correctly to visible elevation.

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiic2-report.md`.

### IIIC.3: Overriding Plate Uplift

Work:

- For each subducting triangle, compute the uplift `ũ_j(p)` per thesis §3.3.1.3 formula:
  `ũ_j(p) = u_0 · f(d(p)) · g(v(p)) · h(z̃_i(p))`
  where `u_0 = 6 × 10⁻¹` mm/yr, `f` is the distance transfer (cubic-piecewise from thesis Figure 6/37), `g(v) = v/v_0`, and `h(z̃) = z̃²` reads `HistoricalElevation` of the subducting triangle.
- Apply `Δz = ũ_j(p) · δt` to the over-plate samples within `r_s` of the subducting triangle's barycenter.
- Update `FoldDirection` per the relative convergence direction.

Exit gate:

- Forced-convergence fixture produces uplift on over-plate continental samples; magnitude matches analytical expectation.
- Zero-motion, single-plate, forced-divergence fixtures produce no uplift.
- Same-seed replay produces byte-identical Elevation field.

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiic3-report.md`.

### IIIC.4: Slab Pull (Opt-In, Default Off)

Work:

- Add a config flag `bSlabPullEnabled`, default `false`.
- When enabled: for each subducting plate `P_i`, modify the geodetic rotation axis `w_i` per thesis §3.3.1.3:
  `w_i(t+δt) = w_i(t) + ε · Σ_k (c_i × q_k / ||c_i × q_k||) · δt`
  where `q_k` are subducting front triangle barycenters and `ε = vs/v0` (with `vs = 8 mm/yr` per Table 3.2). Renormalize `w_i` after.
- Compute two motion hashes: `motion_with_slab_pull_hash` (always) and `motion_no_slab_pull_hash` (under a parallel bypass evaluation).

Exit gate:

- Slab-pull off: `motion_no_slab_pull_hash` matches IIIB-equivalent baseline byte-for-byte.
- Slab-pull on: deterministic across replays. Plate `ω` remains bounded by `v0`.
- Forced-convergence fixture: subducting plate axis migrates toward subduction front per analytical expectation.

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiic4-report.md`.

### IIIC.5: Material Ledger Extension For Subduction Effects

Work:

- Extend the existing material ledger schema to include "subduction uplift" elevation deltas as a named line, separate from the Phase II `consumed_by_subduction` material category.
- Phase II material categories remain unchanged for replay determinism.

Exit gate:

- Material ledger reconciles with the new line.
- Per-event uplift contribution is summable to the per-step Elevation delta sum.

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiic5-report.md`.

### IIIC Consolidation

Work: `docs/checkpoints/phase-iii-iiic-consolidated.md` summarizing IIIC.1-IIIC.5 with the slab-pull on/off differential explicitly documented. Consolidate IIIC.1-IIIC.3 into one normal process-layer preset for future slices while keeping slab pull as a separate opt-in authority-feedback switch.

## Sub-Phase IIID: Continental Collision

Goal: implement paper-faithful continental collision (Slab Break + suture). This is the architectural answer to Slice 5.5's coherent-transfer continental-loss asymmetry.

### IIID.1: Connected Continental Terrane Detection

Work:

- For each collision-candidate contact (from IIIB), perform a mesh traversal on the source plate starting from the collision-candidate triangles.
- Expand through neighbor connectivity while triangles remain continental (`x_C > 0.5`).
- Inner enclosed oceanic regions within a continental connected component are included in the terrane (per thesis §3.3.1.3 step 3a — inner seas).
- Output: terrane triangle set (a connected sub-mesh of the source plate).

Exit gate:

- Detection is deterministic.
- Forced-collision fixtures produce expected terrane sets.
- Pure-oceanic fixtures produce empty terrane sets.

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiid1-report.md`.

### IIID.2: Collision Candidate Grouping And Interpenetration Gate

Work:

- Group collision-candidate contacts by opposing plate pair.
- Compute interpenetration distance per opposing-plate-pair as `max(d(p))` over collision-candidate triangles in the group.
- Gate: collision is accepted only if interpenetration ≥ 300 km (paper threshold).

Exit gate:

- Sub-threshold collision candidates do not trigger collision events.
- Forced-collision fixture with adequate convergence speed reaches threshold within expected timesteps.

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiid2-report.md`.

### IIID.3: Opposing Continental Mass Test

Work:

- For each accepted collision candidate, traverse from collision-candidate destination-plate triangles through continental neighbors to compute reachable continental area.
- Reject collision if reachable continental mass on destination is below a threshold (config; initial value: 50% of source terrane area).

Exit gate:

- Test is deterministic.
- Reverse-collision (small destination continent) is rejected; processing falls through to next plate per thesis §3.3.1.3.

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiid3-report.md`.

### IIID.4: Slab Break Topology Detach (Dry Run)

Work:

- Implement the Slab Break operation as a dry-run: produce a plan describing which triangles will be removed from the source plate, with new index mappings.
- Do not yet mutate plate topology.

Exit gate:

- Plan is deterministic across replays.
- Plan validates: removed-triangle set equals terrane triangle set; surviving topology remains a valid Spherical Delaunay sub-triangulation with empty neighborhood at borders.

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiid4-report.md`.

### IIID.5: Suture Topology Augmentation (Dry Run)

Work:

- Implement the suture operation as a dry-run: produce a plan describing how terrane triangles are added to the destination plate, with new index mappings and boundary recomputation.
- Do not yet mutate plate topology.

Exit gate:

- Plan is deterministic.
- Plan validates: destination plate post-suture is a valid sub-triangulation; boundary tracking can be re-initialized.

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiid5-report.md`.

### IIID.6: Apply Detach + Suture Mutations

Work:

- Apply the IIID.4 and IIID.5 plans to actual plate topology.
- Source plate loses terrane triangles; destination plate gains them.
- Both plates have boundary tracking lists reinitialized.
- Subduction matrix entries involving these plates are invalidated (will be recomputed in next IIIB cycle).
- One collision per timestep maximum.

Exit gate:

- Topology mutations are deterministic across replays.
- Per-event topology delta is captured as an event record.
- Source plate continental area decreases by terrane area; destination plate continental area increases by the same amount (mass conservation through topology transfer).

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiid6-report.md`.

### IIID.7: Collision Uplift Propagation

Work:

- After suture, apply discrete `Δz` to destination plate samples within collision influence radius `r = r_c · √(v(q)/v_0 · 𝒜/𝒜_0)` per thesis §3.3.1.2.
- `Δz(p) = Δ_c · 𝒜 · f(d(p, R))` with `f(x) = (1 - (x/r)²)²`.
- `Δ_c` from Table 3.2 (`1.3 × 10⁻⁵ km⁻¹`).
- Update `FoldDirection` for affected samples.

Exit gate:

- Uplift area equals influence radius area within numerical tolerance.
- Uplift magnitude matches analytical expectation at the influence center.
- No uplift outside influence radius.

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiid7-report.md`.

### IIID.8: Slice 5.5 Asymmetry Recheck

Work:

- Re-run the Slice 5.5 source-triangle-uniformity ledger subdivision with IIID active.
- Specifically measure: net continental delta from single-hit transfer to uniform-oceanic source triangles.

Exit gate:

- Continental loss attributed to single-hit transfer to uniform-oceanic source triangles drops by ≥ 80% relative to the Phase II Slice 5.5 baseline, or the slice pauses for an investigation checkpoint that audits the collision implementation against the paper/thesis before the target is reconsidered.
- Continental mass transferred via collision events accounts for at least 50% of the loss eliminated by collision handling.

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiid8-report.md`.

### IIID Consolidation

Work: `docs/checkpoints/phase-iii-iiid-consolidated.md` summarizing collision implementation, with the IIID.8 quantification as the headline result.

## Sub-Phase IIIE: Divergent Zone / Oceanic Crust Generation

Goal: complete the paper's oceanic crust generation model and reframe the Slice 4/5 ledger interpretation.

### IIIE.1: Q1/Q2 Audit Against Current Implementation

Work:

- Read the current Stage 1.5 q1/q2/ridge-midpoint implementation in detail.
- Compare to thesis §3.3.2.3 step 2b.
- Document discrepancies, if any. If the implementation is faithful, proceed to IIIE.2 with no code changes.

Exit gate:

- Audit report concludes either "implementation matches thesis spec" or lists specific discrepancies for remediation.
- No code changes in this slice.

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiie1-report.md`.

### IIIE.2: Ridge Direction Field Assignment At Gap Fill

Work:

- For each gap-fill sample in the divergent-zone path, compute `RidgeDirection = (p - q_Γ) × p` per thesis §3.3.2.1.
- Re-tangent and normalize.
- Store on the new sample.

Exit gate:

- Forced-divergence fixture: gap-fill samples receive non-zero ridge direction.
- Ridge direction is approximately tangent to the ridge midpoint segment.

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiie2-report.md`.

### IIIE.3: Oceanic Age Initialization At Gap Fill

Work:

- For each gap-fill sample, set `OceanicAge = 0`.
- For all other oceanic samples that survive resampling, increment age by `δt` (this is the per-step age aging — strictly speaking part of IIIG, but included here because it's tied to oceanic-generation provenance).

Exit gate:

- Gap-fill samples have age zero post-resample.
- Surviving oceanic samples have age incremented by the resample interval since previous remesh.
- Continental samples have undefined age (or 0; not consumed).

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiie3-report.md`.

### IIIE.4: Ridge Elevation Profile

Work:

- For each gap-fill sample, compute `Elevation = α · z̄(p) + (1-α) · z_Γ(p)` per thesis §3.3.2.1.
- `α = d_Γ(p) / (d_Γ(p) + d_P(p))`.
- `z_Γ(p)` from the generic ridge profile (Table 3.2: ridge max `z_r = -1 km`, abyssal `z_a = -6 km`).

Exit gate:

- Forced-divergence fixture: gap-fill elevation profile peaks at ridge midpoint, decays toward `z_a` away from ridge.
- Profile shape matches analytical expectation.

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiie4-report.md`.

### IIIE.5: Ledger Reframing

Work:

- Extend the material ledger to add a "new oceanic creation" line, populated by gap-fill samples whose pre-existing `x_C` was below a threshold (truly oceanic territory).
- Add a separate "overwritten by ridge generation" line for gap-fill samples with pre-existing `x_C` above the threshold (continental material lost without prior collision/rifting).
- The latter quantity should be near-zero in stable runs after IIID lands.

Exit gate:

- Ledger reconciles with new lines.
- "Overwritten by ridge generation" continental loss is below the `1e-4` threshold in fixtures with collision active.

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiie5-report.md`.

### IIIE Consolidation

Work: `docs/checkpoints/phase-iii-iiie-consolidated.md` summarizing oceanic generation completeness and the ledger reframe.

## Sub-Phase IIIF: Plate Rifting

Goal: implement paper-faithful plate fragmentation as a discrete event.

### IIIF.1: Plate-Internal Voronoi Split (Dry Run)

Work:

- For a given plate, generate `n` random centroids inside the plate's continental coverage (`n ∈ [2, 4]`).
- Compute Voronoi partition of the plate's triangles using geodesic distance.
- Output: a partition plan describing which sub-plates each triangle belongs to.

Exit gate:

- Plan is deterministic for fixed seed.
- Each sub-plate has at least one triangle.

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiif1-report.md`.

### IIIF.2: Warped Fracture Boundaries

Work:

- Apply coherent noise (perlin/simplex) to the geodesic distance metric used in IIIF.1, producing irregular fracture boundaries.
- Same warp function as initial-state Voronoi plate generation per thesis §3.2.4.

Exit gate:

- Fracture boundaries are not straight geodesic arcs.
- Plan is deterministic for fixed seed.

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiif2-report.md`.

### IIIF.3: Topology Partition / Duplication

Work:

- Apply the IIIF.2 plan: destroy the source plate; create `n` sub-plates by the duplicate / re-index / re-compact operation per §3.2.4.
- Each sub-plate inherits the parent's geodetic motion as a starting value.
- Boundary tracking lists are initialized from each sub-plate's boundary triangles.

Exit gate:

- Topology mutation is deterministic.
- Source plate no longer exists; sub-plates partition its former coverage.
- Total continental coverage post-rifting equals pre-rifting (mass conservation).

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiif3-report.md`.

### IIIF.4: Divergent Motion Assignment

Work:

- For each sub-plate, set rotation axis `w_ij = q_j × c_j` where `q_j` is the centroid-of-other-centroids per thesis §3.3.2.2.
- Initial angular speed is small (a few mm/yr equivalent).

Exit gate:

- Sub-plates receive divergent motions; pairwise relative motion is positive (separating) at sub-plate centroids.

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiif4-report.md`.

### IIIF.5: User-Triggered Deterministic Rift

Work:

- Add a deterministic-trigger path that allows tests/fixtures to rift a specific plate at a specific timestep.

Exit gate:

- Triggered rifts produce reproducible topology mutations.
- Test fixtures can verify the full rift→divergent-motion→subsequent-gap-generation path.

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiif5-report.md`.

### IIIF.6: Stochastic Probability Gate

Work:

- Implement the per-timestep Bernoulli probability `p_i = min(1, p · x̄_C · 𝒜 / 𝒜_0)` per thesis §3.3.2.2 with constant `p` from a config.
- Apply only after global remesh (per thesis §3.3.2.3).

Exit gate:

- Probability is deterministic for fixed seed.
- Long-horizon runs show rifting frequency matching the analytical expectation within statistical bounds.
- `p = 0` config disables rifting entirely.

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiif6-report.md`.

### IIIF Consolidation

Work: `docs/checkpoints/phase-iii-iiif-consolidated.md`.

## Sub-Phase IIIG: Per-Step Elevation Evolution

Goal: implement per-step continental erosion, oceanic dampening, sediment accretion. Last sub-phase before validation because it is global and would mask earlier process bugs.

### IIIG.1: Continental Erosion

Work:

- Per step, for samples with `x_C > 0.5`: `Elevation -= Elevation/z_c · ε_c · δt` with `z_c = 10 km`, `ε_c = 3 × 10⁻²` mm/yr.

Exit gate:

- Magnitude matches analytical formula.
- Continental-only fixture: elevation decays exponentially toward zero (sea level).

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiig1-report.md`.

### IIIG.2: Oceanic Dampening

Work:

- Per step, for samples with `x_C < 0.5`: `Elevation -= (1 - Elevation/z_t) · ε_o · δt` with `z_t = -10 km`, `ε_o = 4 × 10⁻²` mm/yr.

Exit gate:

- Magnitude matches analytical formula.
- Oceanic-only fixture: elevation drifts toward `z_t` from initial values.

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiig2-report.md`.

### IIIG.3: Sediment Accretion In Trenches

Work:

- Per step, for samples in oceanic trenches (defined by `DistanceToFront < 100 km` and `Elevation < z_a`): `Elevation += ε_t · δt` with `ε_t = 3 × 10⁻¹` mm/yr.

Exit gate:

- Trench definition matches the geometric criterion.
- Magnitude matches analytical formula.
- Forced-subduction fixture produces measurable trench infill.

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiig3-report.md`.

### IIIG.4: On/Off Differential Validation

Work:

- Run a fixture with IIIG enabled vs disabled. Assert the per-step elevation differential matches the analytical sum of the three IIIG contributions.

Exit gate:

- Differential matches expectation within numerical tolerance.

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiig4-report.md`.

### IIIG Consolidation

Work: `docs/checkpoints/phase-iii-iiig-consolidated.md`.

## Sub-Phase IIIH: Tectonic-Only Long-Horizon Validation

Goal: prove the Phase III stack is stable over multi-hundred-event horizons before Phase IV.

### IIIH.1: 60k Multi-Hundred-Event Run

Work:

- Run 100–250 events at 60k samples (≈ 200–500 My).
- Capture per-event Auth CAF, Slice 5.5 source-triangle-uniformity breakdown, collision event count, rifting event count, kernel timing.

Exit gate:

- Auth CAF stabilizes (max-min over second half of run within ±2% of mean).
- Same-seed replay produces byte-identical hashes for all artifacts.
- Slice 5.5 continental-loss-from-uniform-oceanic-source bucket is reduced by ≥ 80% relative to Phase II baseline, or the run pauses for an investigation checkpoint before Phase III closeout can proceed.

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiih1-report.md`.

### IIIH.2: Resolution Scaling Spot Check

Work:

- Repeat IIIH.1 at 100k and 250k for shorter horizons (50 events each).
- Confirm equilibrium behavior holds; confirm runtime within paper Table 2 budget plus Phase III event overhead.

Exit gate:

- Auth CAF equilibrium qualitatively matches 60k.
- Kernel timing within paper Table 2 envelope; Phase III event timing reported separately.

Checkpoint artifact: `docs/checkpoints/phase-iii-slice-iiih2-report.md`.

### IIIH.3: Phase III Closeout

Work:

- Write `docs/phase-iii-closeout.md` summarizing all sub-phases, gates passed, residual findings, comparison to Phase II baseline.
- Address: did IIID close the Slice 5.5 asymmetry? Did slab pull behave deterministically? Did long-horizon Auth CAF equilibrate? Did any sub-phase require deviation from this slice plan?

Exit gate:

- Closeout doc accepted by user.
- Phase III declared complete; Phase IV can begin planning.

Checkpoint artifact: `docs/checkpoints/phase-iii-iiih-consolidated.md` plus `docs/phase-iii-closeout.md`.

## Stop Conditions

Pause Phase III and write an investigation checkpoint if any of these appear (these are imported from `docs/phase-iii-pre-mortem.md`):

- replay hash divergence at any sub-phase boundary
- Phase II hash regression at any IIIA slice
- slab-pull-off run produces different motion than the IIIB baseline
- collision events transfer more area than the source plate's continental coverage
- Auth CAF jumps by more than 5% in a single event
- subduction matrix has non-zero entries in step zero of a remesh window
- third-plate contact emits a `Subducting`, `Overriding`, or collision label
- vertex position is mutated radially anywhere in Phase III code
- ridge generation count exceeds subduction consumption count by more than 2× over a window
- IIIH long-horizon run shows Auth CAF monotonic drift over 50+ consecutive events
