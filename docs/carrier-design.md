# Carrier Design

Status: pre-coding deliverable. No production code is approved by this document.

## Research Target

CarrierLab will test whether a clean-room moving plate-local crust carrier can
preserve coherent material transport, projection coverage, and boundary quality
at 60k, 100k, and 250k samples on a unit sphere.

The first implementation scope is deliberately smaller than the full paper:

- no subduction
- no collision
- no rifting
- no uplift
- no erosion
- no terrain beauty
- rigid motion plus projection/resampling diagnostics only

The lab must answer whether the carrier itself works before any tectonic
process is layered on top.

## Authority Rules

Authoritative data:

- plate-local crust geometry
- plate rigid transform
- plate-local crust/material fields
- explicit projection and resampling diagnostics

Projection/output data:

- global sample projection results
- plate id visualization
- material class visualization
- miss, overlap, boundary, and continental-fraction maps
- per-step metric rows and hashes

Forbidden authority:

- persistent global sample ownership as material transport
- hysteresis, retention, backfill, recovery, or anchoring heuristics
- generic miss-to-ocean repair without classifying why the miss happened
- Aurous exporter/control-panel/sidecar infrastructure

## Core Data Primitives

`SphereSample`

- sample id
- unit position
- deterministic area weight
- neighbor ids for diagnostic adjacency

`CarrierPlate`

- plate id
- initial center and current rotation
- local spherical patch geometry
- local triangles and adjacency
- material samples attached in plate-local coordinates
- deterministic motion parameters

`MaterialSample`

- local position on owning plate
- continental weight in [0, 1]
- crust class for visualization
- elevation/thickness/age placeholders only where required for projection
- immutable initial id for drift/mass audits

`ProjectionHit`

- global sample id
- candidate plate id
- candidate triangle id
- barycentric weights
- angular residual
- hit class: exact, boundary-degenerate, tolerance, invalid

`ProjectionResult`

- global sample id
- raw candidate count
- selected plate id, if any
- selected material fields
- classification: exact-hit, true-miss, divergent-gap, numeric-miss,
  topology-hole, true-overlap, convergent-overlap, boundary-degenerate-overlap

## Geometry And Projection

The implementation should use a deterministic unit-sphere sample set and a
spherical triangle mesh. The paper uses Fibonacci sampling plus spherical
Delaunay triangulation. If the implementation chooses an equivalent robust
triangulation method, it must document the substitution and prove Stage 0
coverage before any motion.

Projection algorithm:

1. Rotate every carrier plate by its current rigid transform.
2. For every global sample, enumerate all plate triangles that contain the
   sample under the geometric predicate.
3. Record raw candidates before resolving.
4. Resolve output with a deterministic tie-break only after metrics capture
   raw candidate counts.
5. Transfer material by barycentric interpolation for exact hits.
6. Classify zero-candidate samples before any fill is applied.
7. Emit metrics and maps from the raw and resolved results.

Boundary degeneracy policy before coding:

- A sample exactly on a plate edge/vertex is logged as a
  boundary-degenerate candidate.
- The resolved output uses a deterministic half-open ownership rule.
- The raw degeneracy count remains visible and cannot be hidden inside "pass".
- True Stage 0 failure is any non-degenerate miss or non-degenerate overlap.

This policy exists because a point-on-triangle query can otherwise count a
single shared mesh vertex as multiple hits even when there is no area overlap.

## Miss Classification

Every no-hit sample must be classified as one of:

- `divergent-gap`: bounded by two or more separating plate fronts and expected
  to become new oceanic crust in a later mutation stage
- `numeric-miss`: within a declared angular tolerance of a valid triangle or
  boundary but rejected by the predicate
- `topology-hole`: not near any valid plate frontier and not explained by
  divergence
- `out-of-domain`: invalid input sample, NaN, non-unit position, or corrupted
  mesh

Stage 0 allows none of these except zero-count fields. Stage 1 and Stage 2 may
produce divergent gaps under rigid motion, but they must not silently become
material without a named mutation stage.

## Overlap Classification

Every multi-candidate sample must be classified as one of:

- `boundary-degenerate-overlap`: zero-area edge/vertex degeneracy
- `convergent-overlap`: area overlap caused by rigid plate convergence
- `numeric-overlap`: tolerance artifact around a boundary
- `topology-duplicate`: duplicate triangles or duplicated plate authority

CarrierLab must report both raw multi-hit counts and true area-overlap counts.
If the raw number looks like the Aurous failure signature, the classification
must explain whether it is real area overlap or boundary degeneracy.

## Material Transport Diagnostics

Independent recomputation is mandatory:

- Compute analytic plate rotations from initial transforms and current step.
- Track immutable material ids for continental samples.
- Compute expected centroid motion in plate-local coordinates rotated to world.
- Compute observed projected continental centroid from projection results.
- Compare expected and observed displacement in angular units and km-equivalent.
- Compute material mass from plate-local authoritative samples and from
  projected visible samples separately.

The implementation must not use a value produced by the projection resolver as
the oracle for the same projection resolver.

## Stage Gates

Stage 0: cold-start carrier

- no motion
- no mutation
- every global sample has exactly one resolved plate
- no non-degenerate misses
- no non-degenerate overlaps
- CAF equals initialized continental mass within tolerance
- deterministic hash stable across repeated runs

Stage 1: rigid motion preservation

- rotate plates only
- report raw miss and multi-hit counts each step
- material centroid displacement matches analytic rotation
- authoritative material area conserved
- no NaN/Inf
- compare against Aurous baseline where available

Stage 1.5: named resampling sub-stage

- introduce paper-style global resampling only after Stage 1 rigid projection
  has a written checkpoint and explicit go/no-go
- do not silently blend resampling into Stage 1
- report pre-resample and post-resample coverage, mass, CAF, and drift metrics
- classify every miss and overlap before applying any new-ocean fill policy

Stage 2: long-horizon stability

- 1000 steps
- misses do not grow superlinearly unless explained by geometric divergence
- true overlaps remain bounded or are classified as convergence
- CAF does not collapse as a projection artifact
- visuals show coherent material drift rather than samplewise noise

Stage 3: resolution scaling

- repeat at 60k, 100k, and 250k
- keep stage parameters fixed unless a written gate revision is approved
- compare qualitative verdict across resolutions

Stage 4: verdict

- viable, viable-with-modifications, or not viable
- quantified comparison to Aurous
- architecture implication for Aurous recorded separately from the lab result

## Stage Checkpoint Rule

At the end of every stage, CarrierLab must write a checkpoint report before any
next-stage work begins. The checkpoint must include:

- exact command and seed
- sample count and plate count
- pass/fail status for every gate
- stop-condition review
- metric table and export locations
- comparison to Aurous baselines or "no baseline available"
- explicit recommendation: go, no-go, or revise-gate

The user reviews the checkpoint and records explicit go/no-go. There are no
silent stage transitions.

## Runtime And Memory Stop Bounds

Practical bounds approved for the experiment:

| Resolution | Wall-clock bound | Memory bound |
| --- | ---: | ---: |
| 60k | 10 minutes | 4 GB |
| 100k | 30 minutes | 8 GB |
| 250k | 45 minutes | 16 GB |

Exceeding these bounds is a stop condition for that resolution and must be
reported as part of the checkpoint rather than optimized around silently.
The 250k timing ceiling is anchored to Cortial et al. Section 7.4 Table 2,
which reports 1.24 seconds per step at 250k for the full tectonic process set,
plus episodic oceanic crust and rifting costs. CarrierLab's carrier-only subset
should run faster than the paper's full pipeline, not slower; exceeding the
bound is a research finding to investigate before continuing.

## Pre-Code Review Answers

1. Boundary policy is approved as written. Boundary-degenerate samples are
   logged separately, resolved by deterministic half-open ownership, and cannot
   hide non-degenerate misses or overlaps.
2. Stage 1 is rigid projection first. Global resampling is Stage 1.5, a named
   sub-stage with its own checkpoint.
3. Runtime and memory bounds are 60k within 10 minutes and 4 GB, 100k within
   30 minutes and 8 GB, and 250k within 45 minutes and 16 GB, anchored against
   Cortial et al. Section 7.4 Table 2 rather than Aurous wall-clock behavior.
4. The mesh path is clean-room Fibonacci sampling plus Bowyer-Watson spherical
   Delaunay. Any substitution requires a written checkpoint revision.

## Required Metrics

Per step:

- sample count
- plate count
- seed
- step
- wall-clock time
- memory snapshot
- raw hit count
- raw miss count
- raw multi-hit count
- true miss count
- true overlap count
- boundary-degenerate count
- resolved classified count
- CAF from authoritative plate-local material
- CAF from projected output
- total material mass
- drift expected distance
- drift observed distance
- drift error mean, p50, p95
- NaN/Inf count
- output hash

Per run:

- deterministic hash pair for same-seed replay
- resolution summary
- stop-condition summary
- comparison row against Aurous baselines

## Exports

Per stage and checkpoint:

- PlateId
- material class
- continental fraction map
- miss mask
- overlap mask
- boundary mask
- drift error map
- contact sheet
- metrics CSV or JSONL

Exports are downstream diagnostics. Passing images without raw metrics cannot
pass a stage.

## Aurous Differences

Different from Aurous Prototype C:

- Prototype C uses nearest rotated centers for ownership and decoupled material
  projection.
- CarrierLab tests the paper-family plate-local geometry carrier.
- CarrierLab permits projection misses/overlaps as measured geometry facts
  during motion; Prototype C intentionally removes ownership gaps/overlaps by
  construction.

Different from Aurous failed V6/V9 remesh path:

- CarrierLab is clean-room and must not reuse V6/V9 code.
- CarrierLab separates raw geometry diagnostics from resolution and material
  fill.
- CarrierLab does not use existing Unreal query infrastructure, exporter, or
  repair logic.
- CarrierLab treats failure as evidence, not as a prompt for threshold tuning.
