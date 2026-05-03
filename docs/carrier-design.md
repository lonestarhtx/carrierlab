# Carrier Design

Status: revised pre-Stage-0 design note. The current Stage 0 code is
provisional until the thesis-spec audit below is complete and the readback is
approved.

## Research Goal

CarrierLab's goal is to reproduce the carrier model from the Cortial
paper/thesis faithfully in a clean-room implementation, achieving the
published per-step performance envelope from paper Section 7.4 Table 2 and
producing carrier-layer behavior consistent with the prerequisites for the
paper's published outputs.

This is paper-faithful reproduction with Aurous differential diagnosis, not a
falsification experiment. The paper's algorithm is treated as the gold
standard. The lab's job is to identify the implementation gap that caused
Aurous's earlier paper-faithful prototype to fail.

The paper's morphology figures include subduction, collision, rifting,
oceanic generation, and downstream terrain processes. Visual matching to those
figures is out of scope for CarrierLab. The carrier behaves correctly when its
layer-local invariants hold.

## Reproduction Outcomes

1. Faithful reproduction succeeded. CarrierLab matches Table 2 performance
   envelope and carrier-layer behavior. The diff against Aurous's failed
   prototype is the bug list.

2. Faithful reproduction succeeded mechanically but performance/quality
   diverged. The implementation follows the thesis spec, but measured results
   diverge from published performance or carrier quality. This points to
   environment, query infrastructure, precision, triangulation, or implementation
   overhead.

3. Faithful reproduction blocked by underspecification. A specific paper/thesis
   gap prevents a clean implementation. The result identifies the gap without
   blaming the paper.

## Authority Rules

Authoritative data:

- plate-local duplicated triangulations
- physically updated plate vertex positions during rigid motion
- plate geodetic motion parameters
- plate-local crust/material fields
- pre-resampling and post-resampling plate-local state

Projection/output data:

- global sample projection results
- selected plate id after tie-break
- material visualization layers
- miss, overlap, boundary, drift-error, and continental-fraction maps
- metrics rows, hashes, and checkpoint reports

Forbidden authority:

- persistent global sample ownership as material transport
- ownership persistence, hysteresis, retention, backfill, recovery, repair, or
  anchoring heuristics
- generic miss-to-ocean repair without miss classification
- Aurous exporter, control panel, sidecar, V6, V9, or Prototype A-E code

## Core Data Primitives

`SphereSample`

- deterministic Fibonacci sample id
- unit position on the output/global TDS
- deterministic area weight
- initial diagnostic plate id only

`CarrierPlate`

- plate id
- geodetic rotation axis and angular speed
- duplicated local vertices copied from the global TDS
- duplicated local triangles copied/re-indexed/re-compacted from the global TDS
- empty neighbor slots at plate borders
- plate-local material fields

`CarrierPlateTriangle`

- local vertex indices
- source global triangle id for audit
- boundary flag
- material interpolation inputs

`ProjectionHit`

- global sample id
- candidate plate id and plate-local triangle id
- barycentric weights from ray/triangle intersection
- boundary-degenerate flag
- raw hit class before resolver

`ProjectionResult`

- global sample id
- raw candidate count
- resolved plate id, if any
- selected material fields
- classification: exact-hit, boundary-degenerate-overlap, true-miss,
  divergent-gap, numeric-miss, topology-hole, convergent-overlap,
  numeric-overlap, topology-duplicate, third-plate-intrusion

## Geometry And Query Model

Defaults:

- samples: deterministic Fibonacci lattice
- triangulation: Unreal GeometryCore `FConvexHull3d` over unit-sphere samples,
  using the 3D convex-hull/spherical-Delaunay equivalence
- plate count: 40
- land coverage: 0.3
- seed: 42
- timestep: `dt = 2 My`
- resampling cadence: `DeltaT = (1-alpha)M + alpha m`, with
  `alpha = min(1, vm/v0)`, `M = 128 Ma`, `m = 32 Ma`, and `v0 = 100 mm/y`

The query model is thesis-specific:

1. For each global sample `p`, construct the ray from the planet center through
   `p`.
2. Test that ray against plate-local triangle meshes.
3. Record every raw hit before resolving.
4. Ignore subducting/colliding triangles only in stages where those processes
   exist. They do not exist in Stage 0, Stage 1, or Stage 1.5.
5. Transfer material by barycentric interpolation for resolved exact hits.
6. Classify misses and overlaps before any fill or tie-break is consumed by a
   downstream operation.

Spherical-triangle containment may be useful as a diagnostic cross-check, but
it is not the thesis query model for projection/resampling.

## Boundary And Tie Policy

Boundary policy:

- Exact edge/vertex hits are logged as boundary-degenerate.
- A named half-open ownership resolver is not yet implemented in Stage 1.
- Until that resolver exists and is tested directly, reports must not describe
  boundary resolution as thesis-faithful half-open ownership.
- Raw candidate counts and boundary-degenerate counts remain visible.
- True Stage 0 failure is any non-degenerate miss or non-degenerate overlap.

No-subduction multi-hit policy:

- Stage 1 uses nearest plate centroid.
- Stage 1.5 treats multi-hit resolution as an experimental condition:
  centroid tie-break, synthetic lower-id-subducts labels for convergent
  overlaps, and deterministic seeded random tie-break.
- Equal-distance centroid ties resolve to the lowest plate id.
- Raw multi-hit count is always reported before any policy winner is applied.

These policies characterize the carrier foundation under missing
subduction/collision state; they are not thesis-faithful substitutes for the
thesis filter. Stage 1.5 reports AuthoritativeCAF, ProjectedCAF, gap-fill
behavior, and per-plate area deltas under each policy so the checkpoint can
distinguish process-independent carrier behavior from behavior future
subduction integration owns.

## Stage Split

Stage 0: cold-start carrier

- Build global TDS.
- Build per-plate duplicated topology.
- Project at t=0 through ray-from-origin plate-triangle queries.
- No motion, no mutation.

Stage 1: within-window rigid motion

- Physically rotate all plate-local vertices each step.
- Project against moved triangulations.
- Do not resample.
- Test barycentric interpolation and drift coherence under motion.

Stage 1.5: cross-window resampling

- Trigger at the thesis cadence.
- Run cadence-first before stress tests at 100/200/400 accumulated steps.
- Reuse the original global TDS as the resampling target.
- Transfer current crust data from moved plate-local geometry.
- Fill zero-hit divergent gaps through the q1/q2/ridge-midpoint algorithm.
- Transfer gap-fill material conservatively from q1/q2 boundary crust data in
  Stage 1.5; qGamma provenance is still logged, but this carrier-foundation
  slice does not permit gap fill to destroy continental mass through a deferred
  oceanic-generation interpretation.
- The current q1/q2 search is a discrete boundary approximation: endpoints and
  spherical midpoints from plate-local boundary edges are indexed, then nearest
  samples from two different plates are selected. This is not yet exact
  continuous nearest-boundary provenance, and reports must name it as such.
- A zero-hit sample with no q1/q2 boundary pair is not allowed to inherit prior
  global sample authority silently. The implementation must count this as
  `no_boundary_pair_fallback_count` and gate it to zero before any resampling
  success claim.
- Rebuild plate-local duplicated triangulations after assignment.
- Report authoritative CAF, projected CAF, projected-authoritative delta, and
  per-plate area deltas before and after every resampling event.

Stage 2: long-horizon stability

- Run 1000 steps with periodic resampling.
- Preserve determinism, mass accounting, bounded classified misses/overlaps,
  and plausible CAF.

Stage 3: resolution scaling

- Repeat at 60k, 100k, 250k, and 500k.
- Record whether the qualitative result changes by resolution.

Stage 4: verdict

- Select one reproduction outcome.
- Quantify comparison to Aurous baselines.
- Document implications for Aurous's current C/D/E path.

## Pre-Stage-0 Code Audit Required

The current Stage 0 implementation predates the thesis read. It uses
sample/incident-triangle projection support rather than the thesis's
ray-from-origin triangle intersection.

Before the Stage 0 checkpoint can pass:

1. Replace sample/incident-triangle projection with ray-from-planet-center
   queries against per-plate triangle meshes.
2. Verify per-plate triangulation construction matches thesis sec. 3.2.4:
   duplicated topology, local re-indexing, local compaction, and empty
   neighborhood at borders.
3. Verify the boundary-degeneracy policy is operational under ray queries.
4. Re-run the Stage 0 cold-start test under the corrected implementation.
5. Document the diff between provisional and corrected output in the readback
   and later Stage 0 checkpoint.

The existing Stage 0 code is scaffolding to be revised, not a passing
implementation.

## Diagnostics And Oracles

Every stage gate must use independent recomputation:

- Drift expected value comes from the plate's initial axis, angular speed, and
  accumulated time, not from projection output.
- Mass from plate-local authority and mass from projected output are reported
  separately.
- Determinism is asserted by same-seed replay and stable hashes. Stage 1's
  strided output hash is only a smoke hash; Stage 1.5 must add full projection
  and full plate-authority hashes before any verdict-level determinism claim.
- Miss/overlap classes are counted before any resolver/tie-break.
- Negative controls include zero-motion, single-plate, forced convergence,
  forced divergence, ocean-only, all-continental, and single-sample-feature
  tests.

Aurous comparison row schema:

`{metric, lab_value, aurous_value, aurous_run_id, parameters_match: bool}`

If no Aurous baseline exists, the row must say `no baseline available` and name
the run needed to obtain it.

## Performance Budgets

Budgets are anchored against paper Section 7.4 Table 2 per-step times and
separate simulation kernel time from diagnostic/export time.

| Resolution | `step_kernel_ms` for 1000 steps | `step_with_diagnostics_ms` for 1000 steps | Memory ceiling |
| --- | ---: | ---: | ---: |
| 60k | <= 2 minutes total | <= 5 minutes total | <= 2 GB |
| 100k | <= 5 minutes total | <= 15 minutes total | <= 4 GB |
| 250k | <= 20 minutes total | <= 45 minutes total | <= 8 GB |
| 500k | <= 45 minutes total | <= 90 minutes total | <= 16 GB |

`step_kernel_ms` is the simulation cost per step and should be compared
directly against paper Table 2. CarrierLab's subset of paper processes
(rigid motion plus projection plus diagnostics, with no subduction, collision,
rifting, or oceanic generation) should run faster than the paper's per-step
total. `step_with_diagnostics_ms` includes PNG export, hashing, and metric
computation, which can dominate runtime and is not comparable to Table 2.

If `step_kernel_ms` exceeds paper Table 2 for the same resolution, that is a
finding worth investigating regardless of total runtime.

## Stage 1 Hardening Addendum

Stage 1 did not produce a clean carrier-viability pass. It showed exact
rigid-motion transport in plate-local geometry, but projected coverage degrades
without resampling. At 250k/40/seed-42 step 100, raw miss rate is close to the
Aurous failure memo baseline; raw multi-hit is similar but not exact. At step
400, authoritative CAF remains stable while projected CAF has lost roughly 38%
relative to authority. That distinction is binding for Stage 1.5.

Stage 1.5 implementation is blocked until these hardening items are represented
in the design and checkpoint gates:

- republished Stage 1 tables must show authoritative CAF and projected CAF as
  separate columns
- forced convergence and forced divergence must be numeric pass/fail controls
  with directional expectations, not `inspect` labels
- interpretation must say that Aurous almost certainly had implementation or
  architecture issues, while Stage 1 does not identify which one
- the third explanation remains live: Aurous's resampling path may have both
  failed to close geometric gaps and destroyed material
- Stage 1.5 multi-hit resolution must remain labeled as an experimental policy
  surface, and Stage 1.5 must report per-plate projected area deltas to detect
  centroid or policy bias

Should-fix items allowed to land inside the Stage 1.5 design phase, but before
Stage 1.5 verdict claims:

- strengthen determinism with full projection and full authority hashes
- implement a named half-open boundary resolver, or keep the design explicitly
  aligned with nearest-centroid policy resolution
- add ocean-only, all-continental, single-sample-feature, and perturbed-boundary
  negative controls
- profile Stage 1 kernel time against paper Table 2 before adding q1/q2 nearest
  boundary search
- add construction-time triangulation invariants:
  `Triangles.Num() == 2 * SampleCount - 4`, Euler characteristic 2, no duplicate
  undirected faces, and no nonmanifold edges

Stage 1.5 must explicitly address the top three pre-mortem risks from the
review:

1. Long-window resampling shock: cadence-first runs are judged before 100/200/400
   stress windows.
2. Projected CAF loss at the resampling boundary: authoritative/projected CAF
   delta is a gate, not a narrative note.
3. No-subduction tie-break bias: per-plate projected area deltas and raw
   multi-hit class counts are reported before centroid resolution is trusted.

## Stop Conditions And Investigation Checkpoints

A stop condition pauses stage advancement and triggers a written investigation
checkpoint. It is not automatically a final verdict.

The investigation must record:

- which thesis section was checked
- what the spec says
- what the implementation does
- where they diverge
- what the proposed fix is

Only if the failure persists after thesis-spec audit does it escalate to a
verdict-level finding.

Stop conditions:

- Stage 0 non-degenerate miss or overlap
- Stage 1 Aurous-like high miss plus high multi-hit by step 100
- CAF collapse without a named material mutation
- determinism break
- memory or runtime ceiling exceeded
- any hard Aurous anti-pattern appears

## Repository Hygiene Check

Before Stage 0 resumes:

- confirm git repo is initialized and remote is set
- confirm `AGENTS.md` contains the push-verification rule using `git ls-remote`
- confirm `.gitignore` excludes `Saved/`, `Intermediate/`, `Binaries/`, and
  `DerivedDataCache/`
- confirm the initial commit captures the four pre-coding deliverables
- add a Stage 0 audit commit capturing the revised deliverables, code audit,
  and readback

## Checkpoint Rule

At the end of every stage, CarrierLab writes
`docs/checkpoints/stage-N-report.md` before any next-stage work begins. The
report includes gate-by-gate pass/fail status, raw metrics, Aurous comparison
rows, visual export locations, stop-condition review, and an honest
recommendation.

The user records explicit go/no-go. No stage advances silently.
