# V2 Source Anchor Table

Date: 2026-06-08

Status: pre-code traceability artifact.

Purpose: separate source-grounded PTP/Cortial requirements from inference,
CarrierLab lab policy, historical evidence, and forbidden patterns before V2
implementation begins.

Authority classes:

- `source_explicit`: paper/thesis or extracted local notes state the rule.
- `source_implicit`: source strongly implies the rule but does not spell out
  the exact implementation choice.
- `derived`: engineering consequence of source-explicit rules.
- `source_silent_lab_policy`: source does not decide; implementation would need
  explicit user approval before code.
- `diagnostic_only`: useful measurement or comparison, not simulation behavior.
- `forbidden`: violates CarrierLab clean-room or paper-authority rules.

## Core Carrier Anchors

| ID | Requirement | Authority | Source anchor | V2 interpretation | Gate impact |
| --- | --- | --- | --- | --- | --- |
| SRC-001 | Planet modeled as tectonic plates on a reference sphere | source_explicit | `docs/paper-carrier-extraction.md`, settled carrier specification, planet state | Plates are first-class carrier objects, not just labels on an output texture | V2-0 must create plate records |
| SRC-002 | Crust data evaluated by barycentric interpolation | source_explicit | `docs/paper-carrier-extraction.md`, crust data sampling | Material fields live on triangle vertices/samples and are transferred by barycentric weights | Projection/remesh metrics must include barycentric validity |
| SRC-003 | Plates move; material must move with plate-local geometry | source_explicit | `docs/paper-carrier-extraction.md`, moving authority | Plate-local vertices and material authority rotate together | V2-1 drift oracle required |
| SRC-004 | Global Fibonacci samples and spherical Delaunay are initial substrate | source_explicit | `docs/paper-carrier-extraction.md`, global mesh seed | V2 begins with deterministic global TDS | Stage 0 source-complete |
| SRC-005 | Global TDS is reused as resampling target | source_explicit | `docs/paper-resampling-extraction.md`, one global TDS | The substrate is stable and reused, but not material authority | Remesh target identity hash required |
| SRC-006 | Per-plate triangulations are duplicated, re-indexed, and compacted | source_explicit | `docs/paper-resampling-extraction.md`, plates own duplicated sub-triangulations | V2 plate topology must be local data, not global references masquerading as plate meshes | Topology invariants required |
| SRC-007 | Plate-local vertices rotate rigidly between resamples | source_explicit | `docs/paper-resampling-extraction.md`, plate-local vertices rotate rigidly | Plate vertices are physically updated or analytically equivalent with an auditable transform | V2-1 independent motion oracle |
| SRC-008 | Center-to-point ray-triangle queries drive projection/resampling | source_explicit | `docs/paper-carrier-extraction.md`, ray query; `docs/paper-resampling-extraction.md`, per-vertex sampling | Projection must query plate-local triangles with center-to-sample rays | Spherical containment cannot be primary path |
| SRC-009 | Rigid-only windows naturally produce gaps/overlaps before remesh | source_implicit | `docs/pre-mortem.md`, rigid motion creates real gaps and overlaps | V2-1 must classify gaps/overlaps, not treat all non-coverage as bugs | Motion fixtures require directional classification |

## Remesh And Process Anchors

| ID | Requirement | Authority | Source anchor | V2 interpretation | Gate impact |
| --- | --- | --- | --- | --- | --- |
| SRC-010 | Remesh starts with plate pre-treatment | source_explicit | `docs/paper-resampling-extraction.md`, Step 1 | Boundary recomputation and subducted-triangle exclusion happen before sampling | V2 remesh cannot be standalone |
| SRC-011 | Subducting/colliding triangle intersections are ignored during remesh sampling | source_explicit | `docs/paper-resampling-extraction.md`, Step 2b | Process state filters candidate hits before selection | Post-filter unresolved multi-hit is a no-go/lab policy case |
| SRC-012 | Valid remesh hits transfer crust parameters by barycentric interpolation | source_explicit | `docs/paper-resampling-extraction.md`, Step 2c | Non-gap samples read current moving authority | Remesh metric must separate valid-hit transfer from gap generation |
| SRC-013 | Zero valid intersections are divergent gaps | source_explicit | `docs/paper-resampling-extraction.md`, Step 2d | Zero-hit after process filtering is a physical divergence class when topology is sound | Gap metrics must verify q1/q2 path |
| SRC-014 | Divergent gap fill uses nearest boundary points q1 and q2 from different plates | source_explicit | `docs/paper-resampling-extraction.md`, divergent gap-fill | V2 target is continuous boundary-edge nearest points | Discrete approximation is not primary paper path |
| SRC-015 | qGamma is spherical midpoint of q1 and q2 | source_explicit | `docs/paper-resampling-extraction.md`, qGamma | Ridge midpoint is deterministic geometry | qGamma residual metric required |
| SRC-016 | New oceanic crust parameters are generated in divergent gaps | source_explicit | `docs/phase-iii-paper-process-extraction.md`, oceanic crust generation | Material creation is allowed only in named divergent generation | Material ledger must classify creation |
| SRC-017 | New plate construction partitions global TDS and rebuilds plate-local triangulations | source_explicit | `docs/paper-resampling-extraction.md`, Step 3 | Remesh ends by rebuilding local topology | Topology hash and triangle assignment metrics required |
| SRC-018 | Subduction marks and matrix are reset/invalidated at remesh | source_explicit | `docs/paper-resampling-extraction.md`, Step 4 | Process state has a bounded lifetime | Reset hash required |
| SRC-019 | Rifting test happens after remesh | source_explicit | `docs/paper-resampling-extraction.md`, Step 5; `docs/phase-iii-paper-process-extraction.md`, plate rifting | Rifting is downstream of remesh, not part of V2-0/1/2 | Excluded from first proof |

## Process Layer Anchors

| ID | Requirement | Authority | Source anchor | V2 interpretation | Gate impact |
| --- | --- | --- | --- | --- | --- |
| SRC-020 | Convergence is tracked before remesh | source_explicit | `docs/phase-iii-paper-process-extraction.md`, process ordering | Remesh consumes convergence state; it cannot invent it | V2-2 precedes V2-4 |
| SRC-021 | Subduction polarity depends on crust type and oceanic age | source_explicit | `docs/phase-iii-paper-process-extraction.md`, subduction and obduction | V2 process state must carry material class and oceanic age before remesh filtering can be paper-like | Material schema gate |
| SRC-022 | Continental collision detaches/sutures terranes before remesh | source_explicit | `docs/paper-resampling-extraction.md`, continental persistence mechanism; `docs/phase-iii-paper-process-extraction.md`, continental collision | Continental preservation is a process/topology operation, not a remesh tie-break | Collision fixtures required before full remesh claim |
| SRC-023 | One collision per timestep maximum | source_explicit | `docs/paper-resampling-extraction.md`, continental persistence mechanism | V2 may need event scheduling semantics | Long-run later; not V2-0/1 blocker |
| SRC-024 | Slab pull mutates plate motion | source_explicit | `docs/phase-iii-paper-process-design.md`, IIIC decisions | First process feedback into carrier authority must be isolated and auditable | Separate slab-pull on/off metric |

## Known Source Gaps And Lab Policies

| ID | Decision area | Authority | Source status | Required V2 handling |
| --- | --- | --- | --- | --- |
| POL-001 | Exact edge/vertex ray boundary degeneracy | source_silent_lab_policy | Sources define ray query but not a full half-open degeneracy resolver | Count separately. Non-degenerate misses/overlaps remain fatal. Any resolver must be named. |
| POL-002 | Multiple valid post-filter intersections | source_silent_lab_policy | Paper expects process filtering to remove losing triangles | Gate unresolved. Do not silently choose nearest/centroid/random in primary path. |
| POL-003 | Mixed-vertex global TDS triangle assignment after per-vertex remesh ownership | source_silent_lab_policy | Thesis says partition triangles but not the tie-break for 2/3 mixed assignments | Requires named policy and fixture; not part of first proof. |
| POL-004 | Continuous q1/q2 implementation detail | source_implicit | Source implies nearest boundary points, not sampled owners | Exact continuous edge query is target. Discrete endpoint/midpoint approximation is comparison policy only. |
| POL-005 | Same-step ordering when collision and remesh trigger together | source_silent_lab_policy | Existing extraction lists as open question | Requires explicit scheduler policy before long-run remesh. |
| POL-006 | Minimum destination continental mass threshold for collision | source_silent_lab_policy | Paper gives process shape but some thresholds/config are not fully operational in extraction | Policy cannot be hidden in remesh. |

## Diagnostic-Only Reuse

| ID | Reusable idea | Authority | Limitation |
| --- | --- | --- | --- |
| DIAG-001 | Same-seed replay hashes | diagnostic_only | Checks determinism for covered state only |
| DIAG-002 | Raw hit count before selection | diagnostic_only | Must not decide material by itself |
| DIAG-003 | Contact sheets and map layers | diagnostic_only | Visuals are secondary evidence |
| DIAG-004 | Existing Phase III/IIIE policy ledgers | diagnostic_only | They document past behavior; they do not authorize V2 behavior |
| DIAG-005 | Rock 3 separated buffers/process/output vocabulary | diagnostic_only | Conceptual comparison only, not source behavior |

## Forbidden Patterns

| ID | Pattern | Why forbidden | V2 stop condition |
| --- | --- | --- | --- |
| FORB-001 | Persistent global sample ownership as tectonic authority | Violates CarrierLab clean-room authority rules and plate-local carrier model | Stop implementation |
| FORB-002 | Ownership persistence, repair, recovery, retention, hysteresis, backfill, anchoring | Reintroduces Aurous-style authority drift | Stop implementation |
| FORB-003 | Prior-owner fallback for misses/gaps | Turns fixed output samples into hidden material memory | Stop implementation |
| FORB-004 | Centroid/random/nearest-hit as primary paper remesh selector | Source does not specify this; paper relies on process filtering | No-go for paper-faithful path |
| FORB-005 | Terrain/surface smoothing to hide invalid crust fields | Masks upstream process failure | No-go for downstream stage |
| FORB-006 | Editor visualization driving simulation correction | Makes UI a hidden solver | Stop implementation |

## V2 Source Sufficiency Verdict

| Area | Sufficiency | Verdict |
| --- | --- | --- |
| Stage 0 cold-start carrier | enough source detail | go for design |
| Stage 1 rigid motion/projection | enough source detail | go for design |
| Contact/convergence dry run | enough for fixtures | go for design |
| Subduction/collision process mutation | enough for scoped fixtures, not full world claim | conditional go |
| Remesh integrated with process state | enough for design, not standalone | conditional go |
| Standalone remesh without process state | not source-complete | no-go |
| Full editor tool | depends on spine first | no-go for coding now |
