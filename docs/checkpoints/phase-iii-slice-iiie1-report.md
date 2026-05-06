# Phase III Slice IIIE.1 Report - Remesh Contract Audit

Status: PASS / CONTRACT LOCKED FOR IIIE.2. This is a no-code checkpoint. It does not implement remeshing, q1/q2, gap fill, oceanic generation, topology rebuild, rifting, or optimization.

Repo state verified before audit:

- `git status --short`: clean
- `git rev-parse HEAD`: `43fe7a90eea48e6ee5b40ad5b7a24424adbc6548`
- latest commit message: `Align IIIB forced-divergence polarity gates`

## Verdict

IIIE.2 implementation is unblocked after user go/no-go. The next slice may implement only the continuous q1/q2/qGamma boundary query and its oracle fixtures. The full IIIE remesh remains blocked until each later slice satisfies its own gate: filtered ray source selection, divergent oceanic field generation, plate-local rebuild, process-state reset, and ledger reframe.

No IIIE.1 stop condition fired:

- Stage 1.5 remains foundation characterization, not a solved remesh.
- Stage 1.5 lab-policy behavior is forbidden from the primary IIIE remesh path.
- Pre-IIIE.8 obduction marks are determinable: they are separate `ConvergenceObductionTriangleMarks`, not true subducting marks, and IIIE must filter them through a separate `obduction_pending` reason.
- IIIB/IIIC/IIID entry assumptions remain usable for this no-code contract slice.
- The Pre-IIIE.7 performance result reduced the integrated investigation fixture to about `5.72x` paper Table 2 baseline, but performance remains a tracked implementation concern rather than a blocker for this no-code audit.

## Sources Read

- `AGENTS.md`
- `docs/phase-iii-planning-packet.md`
- `docs/phase-iii-paper-process-design.md`
- `docs/phase-iii-pre-mortem.md`
- `docs/phase-iii-slice-plan.md`
- `docs/paper-resampling-extraction.md`
- `docs/checkpoints/phase-iii-iiid-consolidated.md`
- `docs/checkpoints/phase-iii-pre-iiie-performance-containment.md`
- `docs/checkpoints/phase-iii-pre-iiie-cost-driver-identification.md`
- `docs/checkpoints/phase-iii-pre-iiie4-integrated-remeasurement.md`
- `docs/checkpoints/phase-iii-pre-iiie5-d1-cost-split.md`
- `docs/checkpoints/phase-iii-pre-iiie6-d7-cost-split.md`
- `docs/checkpoints/phase-iii-pre-iiie7-d7-input-pipeline.md`
- `docs/checkpoints/phase-iii-pre-iiie7-iiid7-equivalence.md`
- `docs/checkpoints/phase-iii-pre-iiie7-replay1-followup.md`
- `docs/checkpoints/phase-iii-pre-iiie8-obduction-bridge.md`
- `docs/checkpoints/phase-iii-pre-iiie-thesis-cleanup.md`
- `docs/checkpoints/phase-iii-iiie-entry-thesis-faithfulness-audit.md`
- latest regenerated `docs/checkpoints/phase-iii-slice-iiib4-report.md`
- latest regenerated `docs/checkpoints/phase-iii-slice-iiib5-report.md`
- latest regenerated `docs/checkpoints/phase-iii-slice-iiid6-report.md`
- latest regenerated `docs/checkpoints/phase-iii-slice-iiid7-report.md`

## Contract Answers

### 1. Primary Paper-Faithful Remesh Path

For each global TDS vertex `p`, IIIE must cast a ray from the planet center through `p` against current plate-local triangulations after pre-treatment. Source selection has exactly three legal outcomes:

1. Exactly one valid post-filter hit: barycentrically interpolate crust/material fields from that plate-local triangle and assign `p` to that plate.
2. Zero valid post-filter hits: route to divergent gap fill using continuous q1/q2/qGamma provenance.
3. More than one valid post-filter hit: record an unresolved anomaly and fail the paper-faithful gate for that fixture/run.

There is no primary-path centroid, random, synthetic-subduction, prior-owner, or projection-derived winner.

### 2. Process State Consumed From IIIB/IIIC/IIID

IIIE consumes process state only to determine which candidate triangles are visible or invisible to the remesh ray and which topology already exists before remesh:

- IIIB: active convergence evidence, distance-to-front records, subduction matrix, and polarity/collision-candidate decisions.
- IIIC: true subducting triangle marks, visible/historical elevation split, continuous subduction uplift state, slab-pull-modified plate motion when enabled, and separate obduction-pending marks from Pre-IIIE.8.
- IIID: applied detach/suture topology and post-collision uplift/fold fields; pending collision groups only as collision-in-process filter evidence, not as ownership.

Plate motion `G` is preserved across remesh. Process tracking state is not.

### 3. Triangles Invisible To Remesh Ray Hits

IIIE must filter these before source selection:

- True subducting triangles: entries in `ConvergenceSubductingTriangleMarks`.
- Obduction-pending triangles: entries in `ConvergenceObductionTriangleMarks`, from continental-continental `CollisionCandidate` evidence before the 300 km collision/suture threshold.
- Collision-in-process triangles: source terrane and destination-front triangles in accepted or deferred collision groups that have not yet been resolved into post-suture topology.

IIIE must count filter reasons separately: `subducting`, `obduction_pending`, and `collision_pending`. Obduction-pending evidence must not be hidden by stuffing it into true subducting marks. That separation is the Pre-IIIE.8 bridge contract.

### 4. Subducting, Obducting, Collision-Pending, And Post-Suture State

| State | Meaning | Remesh visibility | Persistence |
|---|---|---:|---|
| Subducting | Oceanic-under-continental or age-selected oceanic-under-oceanic triangle marked by IIIC. | Invisible. | Reset at remesh; subducting triangles do not survive into rebuilt meshes. |
| Obduction-pending | Continental-continental `CollisionCandidate` evidence receiving continuous uplift before collision authorization. | Invisible through its own filter reason. | Reset at remesh unless it becomes topology first. |
| Collision-pending | A continental collision group/terrane that is authorized or deferred but not yet sutured. | Invisible until collision is either applied or explicitly deferred past remesh by a future user-approved policy. | Must not become ownership history; it is in-window process state. |
| Post-suture | Terrane already detached from source and sutured into destination by IIID.6/IIID.7. | Visible as normal destination-plate topology. | Persists as mesh topology and vertex fields; no separate collision history is authority. |

### 5. Multiple Valid Hits After Filtering

Multiple valid hits after filtering are a fail-loud anomaly. IIIE must record at least:

- raw hit count
- filtered hit count
- filter reason counts
- plates involved
- material class combination
- whether third-plate evidence is present

It must not pick a winner by centroid distance, random seed, synthetic-subduction rule, prior owner, smaller area, or current projected plate id.

### 6. q1/q2/qGamma Contract For Divergent Gap Fill

When no valid hit remains:

1. Recompute plate boundaries after excluding invisible triangles.
2. Find q1 as the continuous nearest point from `p` to any visible plate boundary edge.
3. Find q2 as the continuous nearest point from `p` to a boundary edge on a different plate.
4. Compute `qGamma = R * normalize(q1 + q2)`.
5. Assign the new global vertex to the nearer q1/q2 plate, with exact distance and tie state recorded.
6. Set paper oceanic fields: `OceanicAge = 0`, `RidgeDirection = normalize(tangent((p - qGamma) x p))`, and `Elevation` from the thesis ridge blend using q1/q2 boundary elevation interpolation and qGamma ridge profile.

The endpoint/midpoint search may remain only as a diagnostic comparison. It is not authoritative after IIIE.2.

### 7. Forbidden Stage 1.5 Behaviors

The primary IIIE remesh path forbids:

- retaining prior global sample `PlateId` or material fraction when source selection fails
- centroid, random, synthetic-subduction, or any other policy winner for post-filter multi-hit
- no-boundary-pair fallback that lets a sample keep previous authority
- endpoint/midpoint q1/q2 as the authoritative divergent source
- projection-derived ownership becoming carrier authority
- recovery, repair, backfill, retention, hysteresis, or anchoring under any name
- treating maps or visual layers as gate evidence for remesh correctness

Stage 1.5 metrics and policies may remain as comparison/control artifacts only.

### 8. Process State Reset At Remesh

At the end of IIIE remesh, the implementation must reset:

- `ConvergenceSubductingTriangleMarks`
- `ConvergenceObductionTriangleMarks`
- active boundary/convergence lists
- distance-to-front records
- subduction matrix and polarity decisions
- subduction/collision candidate evidence for the old remesh window
- remesh-window audit counters that would otherwise be read as current process state

It must preserve:

- plate geodetic motion `G`
- crust/material fields transferred through barycentric interpolation or generated by divergent fill
- post-suture topology that was already applied before remesh
- external audit logs as logs only, not simulation authority

The next step after remesh must rebuild IIIB/IIIC process state from current geometry. A non-empty subduction matrix at step zero of a new remesh window is a stop condition.

### 9. Collision Authorization And Remesh Cadence On The Same Timestep

Resolved for IIIE as a lab-named paper-aligned ordering decision:

1. Advance motion and convergence tracking for the timestep.
2. Apply IIIC subduction/obduction continuous effects.
3. If a collision is authorized, apply at most one IIID collision/suture/uplift event.
4. Then, if remesh cadence fires on the same timestep, run IIIE remesh.
5. Reset remesh-window process state.
6. Rifting remains after remesh and belongs to IIIF.

Rationale: continental persistence in the paper depends on convergence/collision handling before the next resample. Running remesh before the authorized collision would erase or reinterpret the very process evidence the remesher is supposed to consume, recreating the Stage 1.5 layering failure. Because the thesis does not explicitly specify same-timestep collision/remesh ordering, this is a named lab policy that must be gated.

### 10. Gate/Oracle Structure Needed For IIIE.2+

IIIE.2+ must use independent recomputation, not implementation-produced counters as proof. Required gates:

- continuous q1/q2 closest-point oracle over known spherical boundary-edge fixtures
- qGamma spherical-midpoint oracle
- filtered ray-hit oracle proving subducting, obduction-pending, and collision-pending triangles are invisible for separate reasons
- fail-loud post-filter multi-hit fixture
- divergent gap-fill field oracle for `OceanicAge`, `RidgeDirection`, and `Elevation`
- plate-local rebuild topology oracle for duplicate/re-index/re-compact coverage
- mixed global-TDS triangle assignment oracle, including triple-junction anomaly accounting
- reset oracle proving all remesh-window process state is empty immediately after remesh
- same-timestep collision-then-remesh ordering fixture
- deterministic replay hashes for vertex assignments, remeshed crust fields, topology, q1/q2/qGamma provenance, and process-state reset
- IIIB independent signature regression whenever a slice consumes convergence tracking

## Contract Table

| Paper requirement | CarrierLab current support | IIIE implementation obligation | Gate needed |
|---|---|---|---|
| Cast ray from center through each global TDS vertex. | Stage 0/1 projection and Stage 1.5 remesh paths already use center-ray projection surfaces. | Use this as the IIIE source-selection primitive, with process filtering first. | Fixture with deterministic per-vertex ray audit and hit inventory. |
| Ignore subducting/colliding triangles before source selection. | IIIC has `ConvergenceSubductingTriangleMarks`; Pre-IIIE.8 adds separate `ConvergenceObductionTriangleMarks`; IIID has collision/suture state. | Filter subducting, obduction-pending, and collision-pending triangles before selecting a source; keep reason counts separate. | Forced convergence and cont-cont obduction fixtures with independent expected invisible triangle ids. |
| Use remaining valid hit for barycentric interpolation. | IIIA.4 already propagates new fields through existing resampling. | Interpolate all crust/material fields from exactly one valid post-filter hit. | Analytical barycentric field fixture, including vector retangency. |
| Treat zero valid hits as divergent gap fill. | Stage 1.5 counts gaps and has diagnostic q1/q2 rows, but uses lab policy. | Route zero-hit samples to continuous q1/q2/qGamma oceanic generation. | Forced-divergence fixture with zero valid hits and independent q1/q2/qGamma oracle. |
| q1/q2 are continuous nearest points on two different plate boundaries. | Current primary Stage 1.5 path uses endpoint/midpoint approximation. | Implement continuous closest point on spherical boundary edges; q1/q2 plates must differ. | Edge fixtures covering interior projection, endpoint clipping, different-plate selection, and tie recording. |
| qGamma is spherical midpoint provenance. | Existing notes and partial path know qGamma form. | Use `R * normalize(q1 + q2)` and record provenance. | qGamma oracle with antipodal/near-antipodal guard. |
| Rebuild plate-local topology from global TDS assignment. | Initial construction and IIID suture/detach use duplicate/re-index/re-compact patterns; full IIIE rebuild is not implemented. | Repartition global TDS triangles, duplicate/re-index/re-compact per plate, and preserve border empty-neighborhood semantics. | Topology coverage oracle plus mixed-assignment anomaly counts. |
| Reset subduction/tracking state at remesh. | Reset fields exist and reports show reset serials, but not through full IIIE remesh. | Clear subducting marks, obduction marks, active lists, distance records, matrix, and old-window evidence. | Remesh-end state-empty oracle and next-step rebuild oracle. |
| Preserve plate motion across remesh. | Phase III design and current cadence docs preserve `G`. | Do not reset or recompute plate motion from projection. | Hash plate motion before/after remesh. |
| Rifting after remesh. | IIIF owns rifting; not implemented here. | Leave rifting out of IIIE except handoff state. | Scope gate: no rift behavior in IIIE. |
| No paper multi-hit tie-break after filtering. | Stage 1.5 has lab policies; IIID gates policy-resolved multi-hit count at zero. | If more than one valid hit remains, count and fail. | Multi-hit anomaly fixture by class: same-material, mixed-material, third-plate. |

## Forbidden-Policy Table

| Policy | Stage 1.5 / current diagnostic status | IIIE primary-path decision | Required guard |
|---|---|---|---|
| Prior global sample owner/fraction fallback | Forbidden by clean-room constraints; Stage 1.5 no-boundary fallback is already a stop-condition counter. | Forbidden. | Grep/static guard plus fixture where no-boundary fallback would previously retain authority. |
| Centroid winner | Exists as Stage 1.5/control policy and many commandlet defaults. | Forbidden as source selector. | `PolicyResolvedMultiHitCount == 0` and no call to policy chooser in IIIE primary path. |
| Random winner | Exists as diagnostic policy. | Forbidden. | Deterministic replay plus static guard. |
| Synthetic-subduction winner | Exists as diagnostic policy. | Forbidden. | Static guard; any synthetic label use must be comparison-only. |
| Endpoint/midpoint q1/q2 authority | Existing approximation. | Forbidden after IIIE.2 except diagnostic comparison. | Continuous oracle must drive primary output; discrete approximation rows must be named diagnostics. |
| Projection-derived ownership authority | Projection maps exist for diagnostics/rendering. | Forbidden. | IIIE source assignment hash must derive from ray/q1/q2 contract only. |
| Obduction hidden as subduction | Pre-IIIE.8 deliberately avoided this. | Forbidden. | Separate subducting and obduction-pending filter counters. |
| Silent post-filter multi-hit resolution | Stage 1.5 policy resolves multi-hit. | Forbidden. | Primary gate fails on post-filter multi-hit > 0. |
| Collision history as persistent ownership | IIID has event/audit records. | Forbidden. | Post-suture state is topology only; event logs not read for remesh authority. |
| Maps/images as gate evidence | Map exports are diagnostics. | Forbidden as gate substitute. | Reports must cite numerical/oracle rows before visual evidence. |

## Open Decisions

| Decision | IIIE.1 disposition | Rationale |
|---|---|---|
| Same-timestep collision/remesh ordering | Resolved now: collision/suture/uplift before remesh; reset after remesh. | Paper persistence depends on collision before resample; reverse order recreates Stage 1.5 loss. |
| Obduction-pending mark filtering | Resolved now: filter via separate obduction-pending reason. | Pre-IIIE.8 intentionally created `ConvergenceObductionTriangleMarks` so IIIE does not overload true subducting marks. |
| Post-suture history persistence | Resolved now: no persistent history; sutures are baked topology plus vertex fields. | `docs/paper-resampling-extraction.md` states collision history is not tracked and sutured terranes persist as mesh topology. |
| Mixed global-TDS triangle assignment | Partly resolved: all-same vertices copy normally; 2-of-3 majority assignment is the proposed IIIE.5 lab policy; 1-1-1 triple-junction triangles are unresolved anomalies until explicitly approved. | The thesis names partitioning by vertex plate assignments but does not spell out mixed triangles. The IIIE.5 checkpoint must gate counts and not use prior owner/projection as a tie-break. |
| Continuous q1/q2 target geometry | Resolved for IIIE.2: closest point on spherical boundary edge, clipped to edge arc. | This is the paper-faithful interpretation recorded in `docs/paper-resampling-extraction.md`. |
| Diagnostic q1/q2 approximation | Resolved: endpoint/midpoint may remain only as comparison. | It is not the authority path after IIIE.2. |
| Equal-age ocean-ocean polarity | Deferred: IIIB.5 keeps equal-age ocean-ocean deferred; IIIE must not invent a remesh policy for it. | Equal-age polarity is visible process evidence but not needed for IIIE.2 q1/q2; any future policy needs its own approval. |
| Multi-plate collision/triple-junction breakage | Deferred before IIIH, but IIIE must count third-plate/triple-junction anomalies separately. | The entry thesis audit flags this as an open IIID/IIIH gap, not a blocker for IIIE.2. |

## Stop Conditions For IIIE.2+

Pause and write an investigation checkpoint if any of these occur:

- IIIE code reads prior global sample owner, previous global fraction, or projection map as remesh authority.
- Primary path calls centroid/random/synthetic policy selection.
- Continuous q1/q2 cannot find two different visible plate boundaries and the code retains prior state instead of failing/counting.
- Endpoint/midpoint q1/q2 becomes authoritative after IIIE.2.
- Obduction-pending evidence is merged into true subducting marks.
- Post-filter multi-hit count is silently resolved.
- Remesh leaves subducting marks, obduction marks, active lists, distance records, or subduction matrix non-empty.
- Same-timestep collision/remesh fixture runs remesh before applying an authorized collision.
- Mixed global-TDS triangle handling uses prior owner/projection fallback.
- Any IIIE report retroactively promotes standalone Stage 1.5 to the thesis remesh path or says IIID solved Slice 5.5/remesh preservation.
- Any visual/export artifact is used as gate evidence without an independent numerical gate.
- Replay hashes for q1/q2, vertex assignments, field generation, rebuild topology, or reset are nondeterministic.

## Proposed IIIE.2 Slice Boundary

IIIE.2 should implement only continuous q1/q2/qGamma provenance and tests. It should not yet run the full remesh source-selection path or generate final oceanic crust fields.

Work:

- Add a deterministic closest-point-on-spherical-boundary-edge query.
- Require q1 and q2 to come from different visible plates.
- Compute and record qGamma as `R * normalize(q1 + q2)`.
- Preserve endpoint/midpoint q1/q2 only as diagnostic comparison rows.
- Add fixtures for interior edge projection, endpoint clipping, two-plate selection, no-two-plate anomaly, and replay determinism.
- Include a static/guard check that no Stage 1.5 prior-owner, centroid/random/synthetic, or discrete-q1/q2 authority path feeds the new primary query.

Exit gate:

- Continuous q1/q2/qGamma oracle residuals pass.
- Discrete approximation is reported only as diagnostic delta.
- No forbidden policy is invoked.
- Same-seed q1/q2/qGamma provenance hash is byte-identical.
- Report explicitly preserves the Stage 1.5 reframe and does not claim remesh/material preservation is solved.
