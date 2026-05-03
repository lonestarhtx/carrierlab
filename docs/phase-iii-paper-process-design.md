# Phase III Paper Process Design

Status: draft for user review. This document is an ADR-shaped design contract for Phase III. It is not approval to implement Phase III code. Phase III sub-phases land only after their individual planning entries in `docs/phase-iii-slice-plan.md` are accepted.

## Context

Phase I established the core carrier substrate: Stage 0/1 validate plate-local duplicated topology, ray-from-origin projection, and rigid geodetic motion. Stage 1.5 is now treated as a foundation-characterization slice, not as standalone paper-faithful resampling. The focused reread in `docs/paper-resampling-extraction.md` shows that the paper remesher consumes convergence/collision state as input: subducting and colliding triangles are filtered before resampling rays, and continental persistence is handled by collision-driven terrane transfer before the remesh. Phase II (Slices 0–5 plus 5.5) added the minimum process-coupled subduction machinery needed to make convergent multi-hit resolution explicit and auditable, and showed that paper Table 2 runtime is comfortably within reach for the measured kernel (~6× headroom at 250k samples).

Phase II also produced an unexpected piece of evidence in Slice 5.5: the dominant single-hit continental delta is *not* mixed-triangle barycentric smear (only 168 of 23,967 single-hit records, contributing -0.004 of the -0.375 net at 60k). It is *coherent transfer* — continental samples reading from uniformly-oceanic interior triangles of plates that have moved into their position via geodetic motion, and the converse. The asymmetry (loss exceeding gain by ~0.37) is consistent with continental plates losing area at convergent zones because no collision/suture process exists to transfer continental material rather than lose it.

Phase III's job is therefore not to "fix" the carrier by adding clever projection policy. It is to reconstruct the paper processes whose absence shows up as the Slice 5.5 coherent-transfer asymmetry: continental collision (the load-bearing one), persistent subduction state, the full remesh/oceanic-generation operation, rifting, and per-step elevation evolution.

## Decision

Build Phase III as eight sub-phases (IIIA–IIIH) sequenced storage → tracking → mutation → topology → events → equilibrium:

- **IIIA** — paper crust state schema (storage only, no behavior).
- **IIIB** — convergence tracking (read-only state, no mutation).
- **IIIC** — continuous subduction/obduction (mutation, including slab-pull feedback into carrier authority).
- **IIID** — continental collision (topology mutation; the architectural answer to Slice 5.5).
- **IIIE** — paper remesh / divergent-zone oceanic crust generation (full §3.3.2.3 remesh integration, including filtering, continuous q1/q2, qGamma, and state reset).
- **IIIF** — plate rifting (discrete event).
- **IIIG** — per-step elevation evolution (continental erosion, oceanic dampening, sediment accretion).
- **IIIH** — tectonic-only long-horizon validation (closes Phase III before any Phase IV amplification work).

The sequencing is not negotiable: storage must precede tracking; tracking must precede mutation; non-topology mutation must precede topology mutation; topology mutation must precede discrete events; per-step adjustment passes are last because they are global and would mask process bugs if landed earlier.

## What The Carrier Continues To Own

These authority rules from Phase I/II remain non-negotiable:

- Plate-local duplicated triangulations are crust authority. Global TDS samples are projection/resampling targets, never persistent tectonic authority.
- Per-plate triangulation construction follows thesis §3.2.4: duplicated topology, local re-indexing, local re-compaction, empty neighborhood at borders.
- Resampling at cadence `ΔT = (1-α)M + αm`, `α = min(1, vm/v0)`, `m = 32 Ma`, `M = 128 Ma`.
- Ray-from-origin per global TDS sample for projection/remesh-time intersection. In the paper-faithful remesh path, subducting/colliding triangles are excluded before candidate selection.
- Determinism: same-seed replay produces byte-identical hashes for projection, carrier state, and all process artifacts.
- No persistent global sample ownership as authority. No ownership recovery, repair, backfill, hysteresis, or anchoring.
- No centroid or random tie-break in any primary path. They remain comparison controls only.

## What Phase III Adds As Authority

Phase III introduces process state that lives at two distinct lifetimes:

- **Per-vertex / per-triangle scalar and vector fields** (carried with the plate-local mesh, copied through topology operations): `z` (elevation), `a_o` (oceanic age), `r` (ridge direction), `f` (fold direction). Optionally `e` (thickness) if needed before Phase IV.
- **Persistent process state across events, reset at remesh:** subduction tracking lists, per-triangle distance-to-front `d(p)`, plate-pair subduction matrix, neighbor-propagation frontier. This state is *invalidated* at every remesh (per thesis §3.3.2.3) and rebuilt for the subsequent step window. It is not ownership; it is recomputable evidence with a defined lifetime.

The single Phase III place where *process output feeds back into carrier authority* is **slab pull** (IIIC): the subducting plate's geodetic rotation axis `w` is modified by the integrated effect of subducting triangle barycenters. This is paper-faithful (thesis §3.3.1.3) but it is the first algorithmic loop in CarrierLab where carrier motion depends on process state. It requires its own pre-mortem section, hash gating, and a bypass switch for differential testing.

## Initial Data Model

Names will evolve in code, but the separations are binding.

**Crust state additions** (per `FCarrierVertex` / per-sample, propagated through resampling):

- `Elevation` — scalar km, range approximately [-10, +10].
- `OceanicAge` — scalar Ma, defined only when sample is oceanic.
- `RidgeDirection` — unit 3D vector tangent to the sphere at the sample, defined only when sample is oceanic.
- `FoldDirection` — unit 3D vector tangent to the sphere at the sample, defined only when sample is continental and inside an orogeny.
- `HistoricalElevation` — scalar km, retained for subducting triangles to support uplift calculations on the over-plate.

`OrogenyType` (`o` in the paper, in [0,1]) and `OrogenyAge` (`a_c`) are deferred until Phase IV demonstrates a need. `Thickness` (`e`) is stored if Phase IIIC's uplift formula requires it; otherwise deferred.

**Convergence tracking state** (per-plate, persistent across events, reset at remesh):

- `ActiveBoundaryTriangles` — list of plate-local triangles currently being tracked for convergence.
- `DistanceToFront` — per-active-triangle scalar km, over-estimated per thesis §3.3.1.3 by `d(p, t+δt) = d(p, t) + s(p)·δt`.
- `SubductionMatrix` — pairwise plate flag indicating whether subduction between plates `i` and `j` is currently authorized.
- `SubductingTriangles` — set of plate-local triangles currently marked as in subduction. These are excluded at remesh.

**Collision event records** (per-event, audit-only):

- Source plate, destination plate, terrane triangle set, suture ring, uplift influence radius, post-event topology delta. One collision per timestep maximum.

**Rifting event records** (per-event, audit-only):

- Source plate, fragment count (2–4), Voronoi seeds, fracture warp parameters, new geodetic motions, post-event topology delta.

## Sub-Phase Architectural Decisions

### IIIA — Paper Crust State Schema

**Goal:** add crust-state fields to the carrier vertex/sample without changing any tectonic behavior.

**Architectural decisions:**

- All new fields are added inert: no projection, no mutation, no consumer. Each field is initialized to a defined default (`Elevation = 0`, `OceanicAge = 0`, `RidgeDirection = zero`, `FoldDirection = zero`).
- Hash gating is *additive*, not replacing: the existing Phase II projection / state hashes must remain identical with the new fields zeroed. New fields participate in *new* hashes (e.g. `crust_state_hash`) so the absence of behavior change is explicit at the hash level.
- Vector fields (`r`, `f`) are rotated at each step alongside the existing geodetic rotation of vertex positions. Rotation is the only behavior added in IIIA, and only after the storage and rotation hashes are accepted.
- Barycentric interpolation of new fields through resampling lands as a separate slice within IIIA (after rotation), not bundled with field addition.

**Non-decisions:** IIIA does not decide whether `Elevation` is signed (it is, per the paper). It does not decide whether `OceanicAge` is initialized to zero or to a random value (zero per thesis §3.3.2.1). These are spec choices, captured in the slice plan.

### IIIB — Convergence Tracking (Read-Only)

**Goal:** instrument paper-faithful convergence tracking state without mutating any crust data or filter behavior.

**Architectural decisions:**

- Tracking state is per-plate and persistent across timesteps within a remesh window. At remesh, all tracking state is invalidated and rebuilt.
- Distance-to-front uses the thesis over-estimation formula. Brute-force exact-distance recomputation is not permitted in the primary path (it would dominate runtime); the over-estimate is the paper's choice and is binding.
- Per-active-triangle ray-from-origin-to-barycenter intersection tests against per-plate BVHs are the detection mechanism. This *replaces* nothing in Phase II — Phase II's signed-velocity contact detection at evidence points remains for the remesh-time filter. The two coexist: Phase III tracking is per-step, Phase II contact detection is per-resample-event.
- Polarity rules are evaluated read-only: the rule fires, but no triangle is yet marked as subducting and no filter behavior changes.
- Replay hash includes the full active-triangle set, distance-to-front values, and subduction matrix at each step.

**Non-decisions:** IIIB does not decide what to do when a triangle is marked as subducting (that's IIIC). It does not decide how to handle continental-continental contacts beyond logging them as collision-candidates (that's IIID).

### IIIC — Continuous Subduction / Obduction

**Goal:** mutate plate state per the thesis subduction model, including the slab-pull feedback into carrier authority.

**Architectural decisions:**

- Subducting triangles persist across timesteps within a remesh window. They are excluded from rendering, projection candidacy, and contact detection while marked.
- Overriding-plate uplift writes to `Elevation` on continental over-plate samples per the thesis §3.3.1.3 uplift formula. Per-step contribution is bounded; cumulative uplift accumulates between remesh events.
- Visible-vs-historical elevation: subducting triangles retain a `HistoricalElevation` field separate from their visible (trench-style) `Elevation`. The historical value is what the over-plate uplift formula reads. Visible elevation is what rendering would consume (Phase IV).
- **Slab pull is the load-bearing feedback decision.** A subducting plate's geodetic rotation axis `w` is modified by the integrated effect of subducting-front triangle barycenters per thesis §3.3.1.3. This is the first place in CarrierLab where process state mutates carrier authority. Mitigations:
  - Slab pull is opt-in via a config flag and defaults off until IIIC.4 is explicitly approved. Paper-faithful primary runs turn it on, and the off-state remains a regression target — same-seed runs with slab pull off must produce IIIB hashes.
  - The slab pull magnitude constant `vs` (paper Table 3.2: 8 mm/yr) is hardcoded but exposed as config for sensitivity tests.
  - A separate `motion_with_slab_pull_hash` is computed independently of `motion_no_slab_pull_hash`. Determinism gates check both.
- Fold direction `f` updates on continental over-plate samples per the relative convergence direction.

**Non-decisions:** IIIC does not implement continental collision (continental-continental contacts continue to be logged as candidates, not acted on). It does not implement obduction's distinct uplift profile beyond the unified subduction-or-obduction handling the thesis itself uses.

### IIID — Continental Collision

**Goal:** implement the paper's continental collision algorithm. This is the architectural answer to Slice 5.5's coherent-transfer asymmetry: when continental plates approach, material transfers via Slab Break + suture rather than being lost to oceanic encroachment.

**Architectural decisions:**

- Collision detection groups collision-candidate contacts by opposing plate pair. Minimum interpenetration distance (paper: 300 km) is the trigger.
- Connected-terrane detection on the source plate is a mesh traversal: starting from collision-candidate triangles, expand through neighbor connectivity while triangles remain continental.
- Opposing continental mass test: enough continental mass must exist on the destination plate to anchor the collision. The threshold is a Phase III config; the paper does not state a hard value beyond the 300 km interpenetration trigger.
- Slab Break: the terrane is detached from the source plate by removing its triangles via the same duplicate / re-index / re-compact operation used for initial plate construction (§3.2.4). Source plate topology is mutated in place.
- Suture: the terrane triangles are added to the destination plate's mesh. Destination plate topology is mutated in place. The plate boundary tracking list for both plates is reinitialized.
- Uplift propagation: a discrete `Δz` is applied to destination plate samples within the collision influence radius `r ∝ √(v(q)/v0 · 𝒜/𝒜₀)` per thesis §3.3.1.2.
- One collision per timestep maximum (paper-faithful for performance and topology stability).
- All collision operations produce a named event record. Topology deltas are auditable. The Slice 5.5 single-hit ledger should show the continental-loss asymmetry decreasing once IIID lands; this is a Phase III gate, not just a Phase III hope.

**Non-decisions:** IIID does not implement reverse subduction (the thesis explicitly skips this). It does not implement post-collision tectonic-axis re-orientation beyond what the Slab Break implies.

### IIIE — Paper Remesh / Divergent Zone Oceanic Crust Generation

**Goal:** replace the standalone Stage 1.5 lab-policy remesh with the paper's full §3.3.2.3 operation, including process-state filtering, continuous q1/q2 provenance, divergent oceanic crust generation, plate-local rebuild, and remesh-time process-state invalidation.

**Architectural decisions:**

- IIIE consumes the process state created by IIIB/IIIC/IIID. Subducting triangles and colliding triangles are invisible to remesh rays. The remaining valid intersection, if any, is the remesh source; centroid/random policies are not a primary-path resolver.
- A multi-hit that remains after subducting/colliding filtering is an explicit unresolved/underspecified event. It is counted and gated; it must not fall back to nearest-centroid, random, prior owner, or any other lab policy in the paper-faithful path.
- Zero-hit samples are divergent gaps. q1 and q2 are continuous nearest points on plate boundary edges from two different plates, not merely discrete boundary vertices. qGamma is the spherical midpoint `R(q1+q2)/||q1+q2||`.
- Gap-fill samples receive the paper oceanic-generation fields: `OceanicAge = 0`, `RidgeDirection = (p - q_Gamma) x p` (retangent/normalized), and `Elevation = blend(z_bar, z_Gamma)` per the ridge profile formula.
- After assignment, the global TDS triangles are partitioned by remesh ownership and duplicated/re-indexed/re-compacted into plate-local triangulations. Per-plate geodetic motion `G` is preserved.
- Remesh invalidates subduction marks, convergence tracking lists, and the subduction matrix. They are rebuilt in the subsequent window; they are not persistent ownership.
- Gap-fill samples no longer contribute to the old "continental destruction" framing. They contribute to a "new oceanic creation" line. Pre-existing continental material overwritten by ridge generation is still tracked as an anomaly; after IIID, that quantity should be near-zero because collision/suture should have moved continental terranes before remesh.
- The Slice 4/5 ledger schema is extended, not rewritten. Existing categories remain for replay determinism; the new categories add detail and reframe what was previously described as destruction.

**Non-decisions:** IIIE does not implement oceanic crust dampening or global erosion (IIIG). It does not repair Stage 1.5 in isolation; it integrates remeshing with the Phase III tracking/collision state the paper requires.

### IIIF — Plate Rifting

**Goal:** implement the paper's discrete plate fragmentation model.

**Architectural decisions:**

- Rifting is triggered stochastically per timestep with Bernoulli probability `p_i = min(1, p · x̄_C · 𝒜/𝒜₀)` per thesis §3.3.2.2. User-triggered deterministic rifting is also supported.
- A plate is split into 2–4 sub-plates by Voronoi partitioning of its interior triangles, with warped fracture boundaries (perlin/simplex noise modulating the geodesic distance metric).
- Sub-plates are constructed by the same duplicate / re-index / re-compact operation as initial plate construction. Source plate is destroyed.
- Sub-plates are assigned divergent geodetic motions: `w_ij = q_j × c_j`, where `q_j` is the centroid of the other sub-plate centroids.
- Rifting happens *after* the global remesh per thesis §3.3.2.3, not during.

**Non-decisions:** IIIF does not implement progressive crust tearing. The paper itself treats rifting as discrete fragmentation; the limitation is acknowledged as future work in §3.4.4 of the thesis.

### IIIG — Per-Step Elevation Evolution

**Goal:** implement the §3.3.3 global per-step elevation adjustment pass: continental erosion, oceanic dampening, sediment accretion.

**Architectural decisions:**

- All three are per-step, per-sample, scalar updates to `Elevation`. They are not tied to any contact, label, or event.
- Continental erosion: `z(p, t+δt) = z(p, t) - z(p, t)/z_c · ε_c · δt` for samples with `x_C > 0.5`.
- Oceanic dampening: `z(p, t+δt) = z(p, t) - (1 - z(p, t)/z_t) · ε_o · δt` for samples with `x_C < 0.5`.
- Sediment accretion: `z(p, t+δt) = z(p, t) + ε_t · δt` for samples in oceanic trenches (defined by a `DistanceToFront` test against a subduction-front threshold).
- Constants from thesis Table 3.2: `ε_c = 3 × 10⁻²`, `ε_o = 4 × 10⁻²`, `ε_t = 3 × 10⁻¹` mm/yr.
- IIIG is the last sub-phase before validation because it is global and continuous: landing it earlier would mask process bugs by smoothing every elevation field every step.

**Non-decisions:** IIIG does not implement coherent noise modulation of the adjustment magnitudes (the paper applies low-frequency noise; this is amplification flavor and can defer).

### IIIH — Tectonic-Only Long-Horizon Validation

**Goal:** prove the Phase III stack is stable over multi-event horizons before Phase IV amplification consumes it.

**Architectural decisions:**

- Run length: 100–250 events (≈200–500 My with the 2 Ma timestep). The paper's reference `Star Citizen`-comparable scenarios run ~125 My (≈60 events); 250 My is the published example duration. Multi-hundred-event coverage is the target.
- Resolution coverage: 60k primary + at least one of 100k/250k.
- Determinism: same-seed replay across the full horizon must produce identical hashes for crust state, subduction tracking, collision events, rifting events, and the material ledger.
- Equilibrium check: Auth CAF should equilibrate (oscillate within bounded range) rather than monotonically drift. If it drifts, the cause must be named (likely IIIE oceanic-age effects, IIIG erosion, or a residual collision/suture imbalance).
- Slice 5.5 evidence revisited: the continental-loss asymmetry from Phase II should be dramatically reduced by IIID's collision implementation. A specific Phase III target quantifies this reduction. If the per-event continental-loss-from-uniform-oceanic-source bucket does not drop by the target amount, advancement pauses for an investigation checkpoint before any interpretation changes.
- Performance: kernel time per step plus per-event Phase III event time should remain within the paper Table 2 envelope at the tested resolutions.

**Non-decisions:** IIIH does not produce visual outputs. It does not amplify. It does not compare against geophysical observations. Those are Phase IV/V.

## Phase III Gates

Phase III as a whole is accepted when:

- All eight sub-phases (IIIA–IIIH) have written checkpoints with passed gates.
- IIIH's long-horizon run shows tectonic equilibrium (no monotonic Auth CAF drift) at 60k.
- Slice 5.5's coherent-transfer continental-loss asymmetry is quantifiably reduced by IIID, with an 80%+ reduction in net continental loss attributable to single-hit transfer to uniform-oceanic source triangles as the initial target. Missing the target is an investigation trigger, not permission to weaken the gate.
- Same-seed replay produces byte-identical hashes for all Phase III artifacts at all sub-phase boundaries.
- Performance at 250k remains within paper Table 2 budget for the kernel (Phase III event costs separately reported).
- All collision and rifting events are named, audited, and reproduce on replay.
- No persistent global sample ownership has been introduced. Centroid/random policies remain comparison controls only.
- The slab-pull feedback off/on differential is documented; same-seed runs with slab-pull off match an IIIB-equivalent baseline.

## Diagnostics

Every Phase III sub-phase checkpoint must include:

- Per-sub-phase replay hashes (separate from Phase II hashes).
- Phase II material ledger continues to reconcile, now extended with paper-remesh categories per IIIE.
- New crust-field heatmaps in the visualization actor (`Elevation`, `OceanicAge`, `RidgeDirection` magnitude, `FoldDirection` magnitude).
- Collision and rifting event timelines with per-event topology deltas.
- Subduction tracking state snapshots: active triangle count, distance-to-front histogram, subduction matrix density.
- Slab-pull on/off rotation-axis trajectory comparison.
- Per-resolution timing breakdown: Phase II baseline kernel + Phase III event costs separately.

## Non-Goals

Phase III is not Phase IV. The following are explicitly excluded and would be stop conditions if they appeared in Phase III:

- Camera-dependent mesh subdivision or hyper-amplification (§4.2 of the paper).
- DEM/exemplar collage amplification (§4.1 of the paper).
- Real-time terrain rendering with displaced geometry. The unit sphere remains the visualization geometry through all of Phase III; new fields are heatmap overlays only.
- River network synthesis, vegetation, atmospheric effects, ocean rendering.
- Geophysical realism comparison metrics. (Aspirational/secondary in Phase V.)

Phase III is also not Phase II rescue work:

- No additional carrier numerical hardening beyond what's needed to support new state.
- No new ownership-recovery, hysteresis, or backfill heuristics under any name.
- No centroid/random tie-breaks promoted to the primary path.
- No Stage 1.5 standalone remesh policy promoted to paper-faithful evidence; IIIE is the first paper-remesh integration point.

## Consequences

**If Phase III succeeds:** CarrierLab will have reproduced the full paper tectonic process layer in clean-room form. Phase IV (amplification) becomes the next legitimate target. The Slice 5.5 finding will have been resolved as "missing collision was the answer." The paper's tectonic algorithm will have been measured under conditions the paper itself does not report (per-event continental conservation, deterministic process replay, scaling to 500k).

**If Phase III fails:** the failure should localize to one sub-phase. The most likely failure surfaces (per `phase-iii-pre-mortem.md`) are the slab-pull feedback loop, continental collision becoming an area-fill, or long-horizon Auth CAF drift not equilibrating. Each is detectable at its own checkpoint; none should require unwinding the whole phase.

**Either outcome:** the Stage 0/1 carrier substrate remains useful and
validated within its scope. Stage 1.5 remains preserved as evidence about what
happens when remeshing is isolated from the process state the paper expects.
Phase III is additive on top of that measured substrate, not a retroactive
claim that standalone Stage 1.5 was paper-faithful.
