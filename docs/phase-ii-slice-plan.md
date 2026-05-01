# Phase II Slice Plan

Status: draft for user review. Each slice ends with a checkpoint note and
requires explicit go/no-go before the next slice begins.

## Slice 0: Spec Audit, No-Mutation Harness, And Baseline Performance

Goal: create the Phase II test surface without adding subduction behavior, while
making the accepted Phase I carrier baseline fast enough to support Phase II
experiments at paper-scale resolutions.

Work:

- Add a Phase II commandlet/test harness that reuses the Phase I carrier.
- Reproduce the accepted Stage 1.5 60k baseline in the new harness.
- Add fixtures: zero-motion, single-plate, forced convergence, forced
  divergence, polarity swap, mixed material, all-continental, ocean-only, and
  triple junction.
- Add canonical JSONL schemas for contacts, triangle labels, filter decisions,
  material deltas, and process hashes.
- Add source-hygiene scan for forbidden authority terms in Phase II source
  files.
- Add timing breakdown instrumentation for the actor and harness:
  projection query, BVH build/refit, boundary search, resampling event,
  drift metrics, hashing, render mesh/color update, and HUD/update overhead.
- Optimize the Phase I baseline as output-preserving acceleration, in separate
  commits:
  1. timing instrumentation only
  2. persistent actor render mesh/color update
  3. cached plate topology with rebuild/refit only when topology changes
  4. combined projection BVH with `{plate_id, local_triangle_id}` metadata
  5. deterministic parallel per-sample ray loop
- Forbid "last resolved plate" or prior global sample ownership caches. These
  are ownership-persistence heuristics, not acceleration.

Exit gate:

- Harness builds and runs with subduction disabled.
- Metrics match the Stage 1.5 baseline within documented tolerance.
- Same-seed replay hashes match for carrier state, projection output, and empty
  Phase II event logs.
- Each optimization commit preserves identical projection and state hashes for
  the same seed before and after the change. Any hash divergence rejects that
  optimization even if it is faster.
- The Slice 0 checkpoint reports 60k, 100k, 250k, and 500k before/after timing
  rows, separating kernel time from actor render/HUD time.
- 250k kernel time targets the paper Table 2 envelope: approximately
  `<= 1.2s/step`; actor total has a separate responsiveness target of roughly
  `<= 1.5s/step`.
- No production mutation path exists yet.

Checkpoint artifact:

- `docs/checkpoints/phase-ii-slice-0-report.md`

## Slice 1: Contact Detection

Goal: detect convergent contacts without affecting projection or resampling.

Work:

- Compute signed relative velocity at raw overlap/boundary evidence points.
- Emit contact records with plate pair, evidence point, velocity, margin,
  contact class, and third-plate flag.
- Keep transform/low-margin and third-plate cases explicit.
- Add mirrored direction tests that prove sign awareness.

Exit gate:

- Forced convergence produces contacts above the velocity margin.
- Forced divergence, zero-motion, and single-plate fixtures produce no
  subduction contacts.
- Reversing the forced fixture reverses the signed velocity.
- Third-plate fixture reports third-plate contacts rather than two-plate
  contacts.
- Replay contact log hash is byte-identical.

Checkpoint artifact:

- `docs/checkpoints/phase-ii-slice-1-report.md`

## Slice 2: Polarity And Triangle Labels

Goal: attach auditable process labels to plate-local triangles.

Work:

- Implement conservative polarity:
  oceanic-under-continental, fixture-specified where provided, ambiguous for
  same-material contacts without an approved proxy.
- Treat third-plate contacts as non-labeling evidence outside the Slice 2
  two-plate polarity model. They are not ambiguous polarity and may not emit
  `Subducting` or `Overriding` labels.
- Label local triangles near contacts as subducting, overriding,
  collision-candidate, or ambiguous.
- Record source contact id, local triangle id, source global triangle id,
  signed velocity margin, distance from contact, and label reason.
- Record an explicit third-plate-out-of-scope count for third-plate evidence
  left unlabeled.
- Export contact and label overlays.

Exit gate:

- Fixture polarity labels the expected under plate.
- Polarity-swap fixture swaps the filtered plate.
- Same-material contacts remain ambiguous unless fixture polarity is enabled.
- Third-plate contacts remain unlabeled or explicitly out-of-scope; any
  third-plate-derived `Subducting` or `Overriding` label is a failure.
- Filter candidate area is bounded relative to boundary length.
- No labels appear in zero-motion, divergence, or single-plate controls.
- Every triangle label is traceable to exactly one non-third-plate contact or
  an explicit two-plate ambiguity record.

Checkpoint artifact:

- `docs/checkpoints/phase-ii-slice-2-report.md`

## Slice 3: Resampling Filter Integration

Goal: replace the Stage 1.5 primary convergent resolver with explicit triangle
filtering.

Work:

- Wire `Subducting` triangle labels into the existing resampling filter hook.
- Keep centroid, synthetic, and random policies as comparison controls only.
- Report pre-filter candidates, filtered candidates, post-filter candidates,
  unresolved multi-hit samples, and filter-exhausted samples.
- Run cadence-faithful 60k before any long-window stress case.
- Add a same-pair mixed-signal fixture where one plate pair produces both
  convergent and divergent evidence at different locations on the sphere.
  Filtering must follow local contact ids and triangle labels, not a global
  plate-pair convergence decision.

Exit gate:

- Post-filter non-boundary multi-hit rate is below 2% at 60k, or unresolved
  classes are fully explained.
- Forced-convergence fixture filters the expected under-plate triangles.
- Forced-divergence fixture still uses gap handling and produces no subduction
  labels.
- Same-pair mixed-signal fixture filters only locally labeled convergent
  evidence and leaves divergent evidence unfiltered for gap/resampling logic.
- Authoritative CAF changes only through named material events.
- Same-seed replay reproduces contacts, labels, filter decisions, and
  post-resampling state hashes.

Checkpoint artifact:

- `docs/checkpoints/phase-ii-slice-3-report.md`

## Slice 4: Material Accounting

Goal: make any subduction material effect explicit and reconcilable.

Work:

- Add named material-accounting records for preserved, filtered, consumed, or
  transferred material.
- Track per-plate continental mass, oceanic mass, projected mass, and tagged
  feature drift.
- Keep elevation, uplift, trench rendering, and slab pull out of scope.

Exit gate:

- Global and per-plate material deltas reconcile exactly with event records.
- No material class changes occur without a named event.
- All-continental and ocean-only controls remain stable unless the fixture
  explicitly enables consumption.
- Tagged continental features do not teleport across plates without a recorded
  event.

Checkpoint artifact:

- `docs/checkpoints/phase-ii-slice-4-report.md`

## Slice 5: Resolution Scaling

Goal: run the accepted Phase II path at 60k, 100k, and 250k before considering
500k.

Work:

- Measure kernel time separately from diagnostics/export time.
- Export contact, label, filtered-overlap, unresolved-overlap, CAF, and
  per-plate delta diagnostics.
- Compare against Stage 1.5 rows and Aurous failure-memo baselines where
  parameter-matched.

Exit gate:

- Qualitative outcome is stable across 60k, 100k, and 250k.
- Runtime stays within the CarrierLab budget or the overage is documented as a
  Phase II finding.
- Determinism remains stable across all target resolutions.

Checkpoint artifact:

- `docs/checkpoints/phase-ii-slice-5-report.md`

## Stop Conditions

Pause the slice and write an investigation checkpoint if any of these appear:

- contact or triangle labels depend on previous global sample ownership
- centroid/random policy remains in the primary convergent-resolution path
- determinism breaks
- third-plate intrusion is folded into ordinary two-plate subduction
- third-plate intrusion emits `Subducting` or `Overriding` labels before an
  approved triple-junction model exists
- filtering is keyed by global plate-pair convergence rather than local contact
  evidence
- CAF or material changes occur without a named material event
- triangle filtering becomes a broad area-fill mask
- sign-aware convergence controls produce symmetric divergence/convergence
  output
- performance exceeds the agreed budget before diagnostics/export cost is
  counted
