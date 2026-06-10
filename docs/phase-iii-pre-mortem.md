# Phase III Pre-Mortem

Assume Phase III fails after several sub-phases land. These are the most likely failure modes, ranked by likelihood times blast radius. Each is structured as Symptom / Likely cause / Evidence / Detection.

## 1. Persistent Process State Becomes Hidden Ownership

Symptom: Phase III subduction tracking, collision event records, or rifting history end up being read as authoritative crust state during projection or resampling.

Likely cause: a sub-phase short-cuts by reading "what was this triangle last step" to avoid recomputation, and the short-cut migrates into the primary path.

Evidence:

- replay hash diverges when the previous step's process state is perturbed but the current step's geometry is identical
- changing remesh cadence changes process state in ways that can't be derived from current geometry
- subduction or collision results depend on which prior event window the simulation is in, beyond what the published cadence implies

Detection:

- hash convergence tracking state separately from carrier crust state
- enforce that all process state is recomputable from current plate-local geometry plus the in-window subduction matrix; reject any code path that reads prior projection results as input
- run a perturb-prior-state-replay-current-step test as a Phase III gate

## 2. Slab Pull Feedback Creates Non-Determinism Or Unbounded Drift

Symptom: same-seed runs with slab pull on produce different rotation-axis trajectories than expected, or plate motions accumulate beyond paper-published velocity bounds.

Likely cause: the slab-pull integrator accumulates floating-point error differently across runs (parallel reduction order), or the per-step geodesic-axis renormalization is missing or applied inconsistently.

Evidence:

- `motion_with_slab_pull_hash` differs across replays of the same seed
- plate angular speed `ω` exceeds `v0 = 100 mm/yr` paper bound after multiple events
- slab-pull-off vs slab-pull-on differential is non-deterministic across runs

Detection:

- IIIC checkpoint requires byte-identical `motion_with_slab_pull_hash` across replays
- plate `ω` is asserted bounded at every step; over-budget is a stop condition
- slab-pull magnitude `vs` is logged per timestep with full precision so divergence is debuggable

## 3. Crust State Expansion Breaks Phase II Determinism

Symptom: adding `Elevation`, `OceanicAge`, or vector fields in IIIA causes Phase II hashes to drift even when the new fields are zeroed.

Likely cause: hashing pulls the new fields by accident (e.g., a serializer iterates struct memory rather than named fields), or the rotation of vector fields in IIIA is applied before hashing.

Evidence:

- Phase II projection hash with all-zero new fields differs from the committed Phase II hash
- Phase II material ledger hash drifts when crust struct layout changes
- Hash drift correlates with struct padding, not field values

Detection:

- IIIA's first slice asserts that Phase II hashes match exactly with new fields zeroed and not yet hashed
- new fields enter their own `crust_state_hash` on a separate slice, gated against the Phase II hash regression test
- struct hashing is by named field, not by memory layout

## 4. Continental Collision Becomes A Broad Area-Fill

Symptom: IIID lands and Slice 5.5's continental-loss asymmetry does decrease — but Auth CAF jumps unrealistically, with collision events transferring orders-of-magnitude more area than the source plate's continental terrane should hold.

Likely cause: the connected-terrane mesh traversal expands beyond continental triangles (e.g., includes ocean-internal triangles the terrane "encloses"), or the suture uplift influence radius applies to triangles already on the destination plate rather than only the suture ring.

Evidence:

- per-event continental transfer area exceeds the source plate's continental coverage
- collision uplift modifies elevation across most of the destination plate, not within the influence radius
- post-collision Auth CAF jumps by more than the source terrane's continental fraction

Detection:

- IIID checkpoint reports per-event terrane area, suture ring length, uplift influence area separately
- mesh traversal must be gated by continental-only neighbor expansion, with explicit count of expansion-rejected triangles
- visual export of every collision event's terrane mask, suture ring, and uplift radius

## 5. Subduction Matrix Outlives Its Paper-Faithful Scope

Symptom: subduction tracking state survives across remesh boundaries, contaminating the post-remesh window with stale plate-pair flags.

Likely cause: a developer assumes the subduction matrix is "expensive to rebuild, why throw it away" and adds a continuity short-cut; or the remesh code path forgets to invalidate the matrix.

Evidence:

- subduction events appear in the first step after remesh between plates whose triangles haven't yet had a chance to track convergence
- remesh-event hash includes prior subduction matrix entries
- forced-divergence test post-remesh shows residual subduction labels

Detection:

- remesh code explicitly clears all process state; assertion at remesh end that subduction matrix is empty
- a "remesh-then-step-zero" replay test that asserts step-zero produces no subduction events from any plate pair
- subduction matrix is a member of the per-event-window state struct, not the plate state struct, to make scope visible at the type level

## 6. Surface Processes Mask Process Bugs

Symptom: IIII's per-step elevation adjustments, relaxation, or smoothing hide irregularities that would otherwise have surfaced as detectable process bugs in IIIC/IIID/IIIE/IIIG/IIIH.

Likely cause: IIII lands earlier than planned, or its constants are larger than thesis Table 3.2 specifies, drowning subduction uplift, collision uplift, rift bathymetry, or ridge profile in numerical noise.

Evidence:

- subduction uplift signature in `Elevation` is too small to see against erosion floor
- collision uplift `Δz` is reduced by the erosion in subsequent steps below the discrimination threshold
- ridge profile peaks erode away within a few steps

Detection:

- IIII is the last implementation sub-phase before validation, never landed earlier
- IIII's constants are hardcoded to thesis values; any deviation is a stop condition
- IIIJ validation runs include both IIII-on and IIII-off replays; the on/off differential should match the analytical expected per-step adjustment, not exceed it

## 7. Long-Horizon Auth CAF Drifts Despite Per-Event Determinism

Symptom: IIIJ's multi-hundred-event run shows monotonic Auth CAF drift rather than equilibrium, even though every individual event reconciles in the ledger.

Likely cause: a small per-event imbalance compounds. The most likely sources, in order: ridge generation contributes more new oceanic samples than subduction consumes; collision uplift accumulates without a counterbalancing erosion sink; oceanic dampening drifts elevations toward abyssal plain even where ridges should sustain them.

Evidence:

- per-event continental delta is small (<0.001) but always negative or always positive
- Auth CAF time series is monotonic over 100+ events
- per-plate elevation distributions migrate over time with no event-correlated jumps

Detection:

- IIIJ gate requires Auth CAF stability over the full horizon (defined as max minus min over the second half of the run within a tolerance)
- per-event continental delta histogram should be roughly symmetric around zero by the second half of the run
- ridge-generation count vs subduction-consumption count should approximately balance over a full window cycle

## 8. Sub-Phases Land In Wrong Order

Symptom: implementation work pulls forward — e.g., IIIC subduction effects partially land before IIIB tracking is hashed, or IIID collision lands before IIIA crust fields exist.

Likely cause: developer optimism about combining "small" slices, or pressure to show visible results before the prerequisite read-only state is verified.

Evidence:

- a sub-phase checkpoint references state from a later sub-phase that hasn't landed
- hashes can't be replayed cleanly because the order of landings creates implicit dependencies
- "fixes" to one sub-phase reach back into a previously-checkpointed sub-phase

Detection:

- the slice plan in `docs/phase-iii-slice-plan.md` is binding; out-of-order slices require a written justification and re-checkpoint of any pulled-forward sub-phase
- each sub-phase's checkpoint includes an explicit dependency list of prior sub-phases it consumes; missing dependencies are a gate failure

## 9. Triple-Junction Handling Regresses

Symptom: IIIB's polarity rules or IIID's collision detection silently include third-plate evidence as if it were two-plate.

Likely cause: a developer treats third-plate intrusion as "just another contact" to simplify code, losing Phase II's explicit out-of-scope handling.

Evidence:

- triple-junction fixture produces subducting or overriding labels for third-plate contacts
- collision events include terranes whose source contacts are three-plate
- third-plate-out-of-scope counter goes to zero in runs that should produce non-zero values

Detection:

- triple-junction fixture remains a Phase III gate (not just Phase II)
- Phase III collision detection asserts that all input contacts are two-plate; third-plate inputs are a stop condition, not silently filtered
- third-plate-out-of-scope counter is reported in every Phase III sub-phase checkpoint as a sanity check

## 10. Phase IV Requirements Bleed Into Phase III Scope

Symptom: a Phase III sub-phase adds amplification-style behavior — camera-dependent mesh subdivision, exemplar lookup, displaced geometry rendering — to "make the visualization look right."

Likely cause: pressure to demonstrate Phase III progress with visible terrain rather than scalar-field heatmaps; or a misunderstanding that paper-faithfulness requires visual matching at the Phase III stage.

Evidence:

- new code in Phase III modifies vertex positions radially
- visualization actor introduces height-displaced rendering
- any Phase III sub-phase references exemplar-based amplification

Detection:

- Phase III checkpoints include a "visualization geometry inventory" line: vertex positions must remain on the unit sphere
- any radial displacement is a stop condition
- visualization changes in Phase III are heatmap layers only, not geometry mutations

## Required Negative Controls

Phase III sub-phase checkpoints must demonstrate:

- **Zero-motion**: no subduction tracking events, no collision events, no rifting events, no elevation changes from IIIC/IIID/IIIE/IIIF/IIIG/IIIH. IIII surface processes may apply if elevation is non-zero, but only in IIII-on runs.
- **Single-plate**: no contacts, no collisions, no subduction matrix entries, no rifting (single plate cannot rift to itself in IIIG; needs at least n=2).
- **Forced divergence**: gap-fill/oceanic-generation events fire; no subduction events; no collision events.
- **Forced convergence (oceanic-continental, fixture polarity)**: subduction tracking with the oceanic plate as under; uplift on continental over-plate; slab pull modifies oceanic plate motion (with slab-pull on).
- **Forced convergence (oceanic-oceanic, age-differentiated fixture)**: older oceanic plate subducts under younger; polarity flip when ages flip.
- **Forced convergence (continental-continental)**: collision-candidate state during IIIB; collision event during IIID; terrane suture; no subduction labels from this contact.
- **Polarity swap**: filtered/under plate swaps when fixture polarity reverses.
- **Same-pair mixed-signal**: a plate pair with both convergent and divergent local evidence; subduction labels apply locally to convergent evidence; gap-fill applies locally to divergent evidence.
- **Triple junction**: third-plate contacts produce no `Subducting`, `Overriding`, or collision labels; third-plate-out-of-scope counter is non-zero.
- **Slab-pull on vs off**: identical IIIB hashes in the off case; deterministic but distinct trajectories in the on case.
- **Multi-event long horizon**: Auth CAF equilibrates rather than monotonically drifts (IIIJ gate).

## Stop Conditions

Pause Phase III and write an investigation checkpoint if any of these appear:

- replay hash divergence at any sub-phase boundary
- Phase II hash regression at any IIIA slice
- slab-pull-off run produces different motion than the IIIB baseline
- collision events transfer more area than the source plate's continental coverage
- Auth CAF jumps by more than 5% in a single event
- IIIE's primary remesh path promotes Stage 1.5 lab-policy behavior
  (centroid/random/synthetic tie-break, prior-owner fallback, or discrete
  endpoint/midpoint q1/q2 as the authoritative source) instead of the
  paper-remesh contract in `docs/paper-resampling-extraction.md`
- subduction matrix has non-zero entries in step zero of a remesh window
- third-plate contact emits a `Subducting`, `Overriding`, or collision label
- vertex position is mutated radially anywhere in Phase III code
- ridge generation count exceeds subduction consumption count by more than 2× over a window
- IIIH long-horizon run shows Auth CAF monotonic drift over 50+ consecutive events
