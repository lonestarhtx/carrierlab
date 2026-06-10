# CarrierLab V2-0 Stage 0 Entry Packet

Date: 2026-06-08

Status: proposed entry decision. No production code has been written for this
packet.

Verdict: `GO_BUILD_V2_0_AFTER_EXPLICIT_USER_GO`, with a narrow boundary. Do not
patch the current Phase III/IIIE path into V2. Do not treat the existing Stage 0
implementation as the V2 kernel. V2-0 should be a small, new, numeric-only
evidence spine that can fail before motion, remesh, UI, or maps enter the
project.

## 1. Current Goal

The current high-level project goal is to recreate the Cortial et al. PTP
moving, plate-local crust carrier model in Unreal Engine 5 as an in-editor tool.

The immediate V2-0 goal is smaller:

1. Build a cold-start carrier from a global substrate into plate-local
   triangulations.
2. Keep plate-local carrier state separate from global projection samples.
3. Project at `t = 0` by center-to-sample ray intersection against plate-local
   triangles.
4. Demonstrate, through small fixtures and negative controls, that the
   implementation can detect holes, overlaps, boundary degeneracy, and
   accidental global-owner authority.
5. Stop with a checkpoint report and wait for explicit user go/no-go before
   Stage 1.

V2-0 is not trying to validate plate motion, remeshing, subduction, collision,
rifting, uplift, erosion, terrain quality, editor workflow, or visual output.

## 2. Inputs Reviewed

This packet consolidates the current rebaseline packet:

- `docs/rebaseline/v2-ptp-editor-tool-proposal.md`
- `docs/rebaseline/v2-source-anchor-table.md`
- `docs/rebaseline/v2-state-transition-model.md`
- `docs/rebaseline/v2-fixture-catalog.md`
- `docs/rebaseline/v2-metric-schema.md`
- `docs/rebaseline/v2-design-checkpoint.md`

It also depends on these source and guardrail documents:

- `docs/paper-carrier-extraction.md`
- `docs/driftworld-carrier-comparison.md`
- `docs/carrier-design.md`
- `docs/pre-mortem.md`
- repo `AGENTS.md` instructions supplied in this thread

Existing code was inspected only to define the implementation boundary and avoid
reusing the wrong foundation. It is not V2 authority.

## 3. Blunt Design Audit

### 3.1 What Looks Recoverable

V2-0 is recoverable if it is treated as a clean V2 evidence spine, not a continuation
of the current Phase III/IIIE code. The paper/thesis source material is strong
enough for a cold-start carrier entry slice:

- global spherical substrate and plate domains exist as initialization inputs;
- per-plate duplicated/reindexed triangulations are source-grounded;
- material attached to local plate geometry is source-grounded;
- projection/resampling by center-to-sample ray against plate-local triangles is
  source-grounded;
- global samples are query/resampling targets, not tectonic authority.

That is enough to build V2-0.

### 3.2 What Is Not Recoverable As-Is

The current integrated code path should not be the V2 foundation. It already has
too much Phase II/III/IIIE behavior nearby: process state, remesh scaffolding,
visualization, commandlet reporting, map export, and historical diagnostics. Even
if pieces are correct, the project failure mode has been integration bloat. V2-0
must therefore start in new V2 files with small structs, small fixtures, and hard
gates.

### 3.3 Main Contradiction Found

The V2 docs correctly say "global samples are not authority", but the existing
project history contains many places where global projected state became the
thing later systems leaned on. The entry packet resolves this by making
`projection_reads_global_owner_count` a hard gate and by forbidding writes from
projection output back into carrier authority during V2-0.

### 3.4 Main Weak Anchor Found

Boundary degeneracy is not fully specified by the paper/thesis at the engineering
tolerance level needed for code. That is not fatal for V2-0 if the behavior is
limited to classification and reporting. It becomes fatal if boundary policy
chooses persistent material, owner, repair, or remesh behavior.

### 3.5 Main Gate Risk Found

A report that says a metric is bad is not enough. V2-0 commandlet success must be
computed from the metric predicates. Negative controls must be considered passing
only when they fail for the expected reason.

## 4. V2-0 Authority Rules

These rules are binding for the first implementation slice.

| Area | Authority in V2-0 | Forbidden in V2-0 |
|---|---|---|
| Global substrate | fixed query/resampling target and initialization mesh | persistent owner/material authority |
| Plate carrier | plate-local duplicated geometry and material records | hidden dependence on projected owner |
| Projection | raw ray-hit candidates and fixture verdicts | repairing, retaining, anchoring, backfilling |
| Material | data carried by plate-local vertices/triangles | material chosen from global sample owner |
| Boundary degeneracy | counted and classified | used to excuse nondegenerate overlap |
| Negative controls | expected-failure fixtures | automatic hole/overlap correction |
| Reports | gate evidence | visual artifact or prose-only pass |

Stop condition: if the easiest way to make V2-0 pass is to add ownership repair,
nearest-owner fallback, centroid selection, prior-owner hysteresis, or projection
authority, the slice fails and Stage 1 is blocked.

## 5. Named V2-0 Policies Needing Approval

These are small, explicit policies. They are allowed into V2-0 only after the
user accepts this entry packet.

| Policy | Authority class | Behavior | Risk | Required audit field |
|---|---|---|---|---|
| `V2-0-POL-BOUNDARY-DEGENERACY-CLASSIFY` | source_silent_lab_policy | classify exact edge/vertex multi-hits separately from true overlaps | hiding real overlap as boundary noise | `boundary_degenerate_count`, per-sample classification |
| `V2-0-POL-RAY-EPSILON` | source_silent_lab_policy | use one documented ray/triangle tolerance for finite precision | tolerance tuned after failure | `ray_epsilon`, `epsilon_policy_id` |
| `V2-0-POL-FIXTURE-SUBSTRATES` | diagnostic_only | allow tiny handcrafted fixture substrates for exact positive/negative controls | fixture passing mistaken for production substrate evidence | `fixture_substrate_id`, `fixture_scope` |
| `V2-0-POL-NEGATIVE-CONTROL-CORRUPTION` | diagnostic_only | fixture harness may remove or duplicate a triangle to verify gates fire | synthetic evidence described as natural geometry | `expected_failure_reason`, `observed_failure_reason` |
| `V2-0-POL-PARTITION-DEFERRED` | source_silent_lab_policy | production-like random/seeded plate partition is deferred; V2-0 fixtures use explicit partition definitions | accidentally validating a made-up seeding algorithm | `partition_policy_id` |

Important: none of these policies may decide persistent tectonic authority. They
only shape diagnostics and fixture construction for V2-0.

## 6. Implementation Boundary

V2-0 may add new files only. It should not modify Phase III/IIIE behavior,
editor UI, visualization actors, existing map exporters, or current checkpoint
reports.

Allowed new files:

- `Source/CarrierLab/Public/CarrierLabV2Core.h`
- `Source/CarrierLab/Private/CarrierLabV2Core.cpp`
- `Source/CarrierLab/Public/CarrierLabV2Stage0Commandlet.h`
- `Source/CarrierLab/Private/CarrierLabV2Stage0Commandlet.cpp`
- `Source/CarrierLab/Private/Tests/CarrierLabV2Stage0Tests.cpp`
- `docs/checkpoints/v2-stage-0-report.md`

Allowed existing-file edits:

- minimal build/module registration only if Unreal requires it;
- no editor-panel, visualization, Phase III, Phase IIIE, or current Stage0 logic
  edits.

Recommended namespace:

- `CarrierLab::V2`
- `CarrierLab::V2::Stage0`

Do not extend the existing `FCarrierState` or `FCarrierLabStage0` as the V2
carrier. Those names are too entangled with historical stages.

## 7. Required V2-0 Structs

The names may vary slightly during implementation, but the boundaries may not.

| Struct | Purpose | Must not contain |
|---|---|---|
| `FCarrierV2Stage0Config` | fixture id, sample count, tolerance, seed, output path | remesh, motion, process, UI settings |
| `FCarrierV2SubstrateSample` | global query sample id, unit position, area weight | plate owner authority |
| `FCarrierV2SubstrateTriangle` | global substrate triangle ids and source sample ids | material authority |
| `FCarrierV2MaterialRecord` | minimal material class/provenance values carried by plate-local data | projected owner fallback |
| `FCarrierV2PlateVertex` | local vertex id, source sample id, unit position, material record | global mutable owner |
| `FCarrierV2PlateTriangle` | local triangle id, source triangle id, local vertex ids | repair/backfill flags |
| `FCarrierV2Plate` | plate id, local vertices, local triangles, source-to-local lookup | global sample authority |
| `FCarrierV2ProjectionHit` | raw ray hit: sample id, plate id, triangle id, barycentrics, hit distance | selected persistent owner |
| `FCarrierV2ProjectionResult` | per-sample classification and optional diagnostic selected hit | carrier mutation |
| `FCarrierV2Stage0Metrics` | counts, hashes, gate booleans, verdict | visual/map gate evidence |
| `FCarrierV2FixtureResult` | expected vs observed fixture outcome | self-reported pass without gate predicates |

Minimum material in V2-0:

- `MaterialClass`: oceanic, continental, mixed, unknown.
- `ContinentalFraction`: scalar fixture value.
- `Provenance`: fixture-defined, substrate-derived, or invalid.

V2-0 does not evolve material. It only demonstrates that material can live on
plate-local state and project back diagnostically without becoming global
authority.

## 8. Required V2-0 Fixtures

All fixtures must be deterministic and small enough to debug by inspection.

| Fixture | Expected result | Hard fail if |
|---|---|---|
| `FX-000 SinglePlateIdentity` | every non-boundary sample has exactly one hit on plate 0; material projects with zero delta | any nondegenerate miss/overlap, projection reads global owner, material authority lives only on global samples |
| `FX-001 TwoPlateCleanPartition` | samples away from the explicit boundary hit exactly one expected plate; distinct material remains distinct | true overlap/miss away from boundary, boundary cannot be separated from overlap |
| `FX-002 BoundaryDegeneracyPin` | edge/vertex samples are classified as boundary-degenerate; nondegenerate miss/overlap remains zero | boundary multi-hit counted as true overlap, or policy claims paper authority |
| `FX-003 DeliberateTopologyHole` | fixture verdict fails for expected miss/topology-hole reason | hole is filled, repaired, ignored, or counted as pass |
| `FX-004 DeliberateDuplicatedOverlap` | fixture verdict fails for expected overlap/duplicate reason | overlap is hidden by centroid, nearest, prior owner, or deterministic tie-break authority |

Fixtures `FX-003` and `FX-004` are negative controls. The commandlet as a whole
passes only if these fixtures fail for their expected reasons.

## 9. Required Metrics

V2-0 must emit one JSONL row per fixture and one summary row.

Required common fields:

- `run_id`
- `stage_id = V2-0`
- `fixture_id`
- `fixture_kind`
- `sample_count`
- `triangle_count`
- `plate_count`
- `ray_epsilon`
- `partition_policy_id`
- `fixture_substrate_id`
- `input_hash`
- `carrier_hash`
- `projection_hash`
- `metrics_hash`
- `build_substrate_ms`
- `build_plate_local_ms`
- `projection_kernel_ms`
- `metrics_ms`
- `report_ms`
- `peak_memory_mb`

Required Stage 0 fields:

- `global_sample_count`
- `global_triangle_count`
- `local_plate_vertex_count_sum`
- `local_plate_triangle_count_sum`
- `topology_duplicate_error_count`
- `topology_hole_error_count`
- `raw_hit_count_total`
- `nondegenerate_miss_count`
- `nondegenerate_overlap_count`
- `boundary_degenerate_count`
- `boundary_policy_selected_count`
- `projection_reads_global_owner_count`
- `material_authority_projected_delta`
- `expected_failure_reason`
- `observed_failure_reason`
- `fixture_pass`
- `stage_gate_pass`
- `verdict`

Forbidden metric shape:

- no "pass" metric that is only a string written by the report builder;
- no hash-only gate standing in for expected-value comparison;
- no metric derived from a previously selected projected owner;
- no visual/map artifact counted as evidence for V2-0 pass.

## 10. Performance Requirements

Performance is part of the design, not a later polish task.

Paper anchor:

- Paper Section 7.4 Table 2 reports a 250k-sample, 40-plate, 0.3-land-coverage
  full-pipeline per-step baseline of about 1.24 seconds.

V2-0 implication:

- Tiny fixtures are correctness microscopes only. They are not performance
  evidence.
- V2-0 must separate kernel timing from diagnostics/reporting. The relevant
  kernel fields are `build_substrate_ms`, `build_plate_local_ms`, and
  `projection_kernel_ms`.
- V2-0 must record `metrics_ms`, `report_ms`, and `peak_memory_mb`, but those are
  not comparable to paper Table 2 because diagnostics can dominate runtime.
- The first serious V2-0 scale gate is 50k samples.
- 250k is the main comparison scale against the paper baseline.
- 500k is the target confidence scale, not the first debug target.

Required V2-0 scale ladder:

| Ladder step | Purpose | Required before |
|---|---|---|
| Micro fixtures | catch wrong authority, holes, overlaps, boundary classification failures | any scale claim |
| 50k | minimum meaningful paper-scale carrier check | recommending Stage 1 |
| 250k | compare practical kernel timing and memory against paper-scale expectations | claiming serious foundation readiness |
| 500k | golden-goose confidence target | claiming high-resolution viability |

Performance no-go conditions:

- 50k cannot complete without changing carrier authority rules;
- kernel timing is mixed with PNG/report/export timing;
- 250k kernel time is slower than the paper full-pipeline baseline without a
  documented cause;
- memory growth suggests accidental per-sample/per-plate quadratic storage;
- performance is achieved by caching global sample owners as persistent
  authority.

If a correctness fixture passes but the 50k scale gate fails pathologically, the
V2-0 report verdict must be `REVISE_V2_0`, not `GO_STAGE_1`.

## 11. Pass/Fail Predicate

V2-0 commandlet exit code must be controlled by the same predicates printed in
the report.

Positive fixture pass requires:

1. `projection_reads_global_owner_count == 0`
2. `topology_duplicate_error_count == 0`
3. `topology_hole_error_count == 0`
4. `nondegenerate_miss_count == 0`
5. `nondegenerate_overlap_count == 0`
6. `material_authority_projected_delta == 0`
7. all expected fixture tokens match independently recomputed fixture facts

Negative fixture pass requires:

1. `projection_reads_global_owner_count == 0`
2. observed failure reason equals expected failure reason;
3. the fixture is not repaired, hidden, or converted into success;
4. the commandlet summary treats the expected failure as a successful negative
   control, not as a production pass.

Stage pass requires:

1. `FX-000`, `FX-001`, and `FX-002` positive fixtures pass;
2. `FX-003` and `FX-004` negative controls fail for expected reasons;
3. replay A/B carrier/projection/metrics hashes match from independent reruns;
4. report rows, predicate booleans, and commandlet exit code agree.

Any missing required metric is a stage failure.

## 12. Independent Oracle Requirement

Tests and commandlets must not validate the implementation by asking the
implementation whether it was correct.

V2-0 needs at least one independent oracle path:

- fixture definitions compute expected plate/material facts directly from the
  fixture input tables;
- projection output is compared against those fixture facts;
- negative controls compare observed failure reasons against fixture corruption
  records;
- replay A/B reruns must rebuild state independently, not reuse objects.

The oracle may share primitive math helpers, but it may not reuse the selected
projection result as the source of expected truth.

## 13. Commandlet Contract

Commandlet name:

- `CarrierLabV2Stage0`

Default behavior:

1. run `FX-000` through `FX-004`;
2. emit JSONL metrics;
3. emit a Markdown report;
4. return `0` only if the stage pass predicate is true;
5. return nonzero if any required metric, fixture, replay, or gate is missing.

Output location:

- `Saved/CarrierLab/V2/Stage0/<timestamp>/metrics.jsonl`
- `Saved/CarrierLab/V2/Stage0/<timestamp>/v2-stage-0-report.md`

Committed checkpoint location after review:

- `docs/checkpoints/v2-stage-0-report.md`

The commandlet must not emit map exports as gate evidence in V2-0. Diagnostic images can
be added later only after numeric gates pass and only as observation.

## 14. Automation Test Contract

Required initial test filter:

- `CarrierLab.V2.Stage0.EntryFixtures`

The test should run the same core fixture logic as the commandlet at tiny scale
and assert:

- all five fixture verdicts;
- required metrics are present;
- positive fixture gates are included in final pass;
- negative control expected failures are included in final pass;
- replay A/B uses independent rebuilt state;
- any attempt to read global owner authority increments
  `projection_reads_global_owner_count`.

The test should not depend on editor UI, map output, or current Phase III/IIIE
state.

## 15. V2-0 Checkpoint Report Template

`docs/checkpoints/v2-stage-0-report.md` must contain:

1. scope and non-scope;
2. exact commit SHA and command line;
3. fixture table with expected/observed/verdict;
4. metric table with hard gates;
5. performance ladder table with micro, 50k, 250k, and 500k status;
6. source-silent policy ledger;
7. replay A/B hash table;
8. independent oracle explanation;
9. known limitations;
10. blunt verdict: `GO_STAGE_1`, `REVISE_V2_0`, or `STOP_PATH`;
11. explicit user go/no-go request.

No later stage may start before this report exists and the user gives explicit
go/no-go.

## 16. V2-0 No-Go Conditions

Do not build or continue V2-0 if any of these become true:

- the implementation needs existing Phase III/IIIE process state to pass;
- projection output becomes carrier authority;
- a global sample owner is stored and reused as tectonic truth;
- hole/overlap failures are repaired during V2-0;
- boundary degeneracy is used to excuse nondegenerate overlaps;
- negative controls pass because the code hides their corruption;
- report pass/fail is not wired to commandlet exit code;
- maps or visuals are used as gates;
- performance timing hides kernel cost inside diagnostics/export/reporting;
- a source-silent policy changes simulation output without explicit approval.

## 17. First Implementation Slice

If the user says go, the next coding slice is:

1. add V2 core structs and fixture builders in new V2 files;
2. implement local plate triangulation construction for the tiny fixtures;
3. implement `t = 0` ray projection against plate-local triangles;
4. implement metrics and hard gate predicates;
5. implement performance timing and memory reporting;
6. implement commandlet and tiny automation test;
7. run the test/commandlet;
8. run the scale ladder at micro and 50k before any Stage 1 recommendation;
9. write `docs/checkpoints/v2-stage-0-report.md`;
10. stop for user go/no-go.

No rigid motion. No remesh. No maps. No editor actor. No UI.

## 18. Final Recommendation

Build V2-0, but only under this packet.

The project is not too much of a mess to recover if V2 starts here. The current
integrated path is too tangled to trust as the foundation, but the source-backed
carrier problem is still small enough to isolate. The first implementation slice
should be deliberately boring: tiny fixtures, raw metrics, hard gates, one
commandlet, one checkpoint, then stop.

If V2-0 cannot pass without repair heuristics or global ownership authority, the
correct result is not another patch. The correct result is `STOP_PATH` or
`REVISE_DESIGN`.
