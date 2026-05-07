# Phase IIIE.5 Topology Rebuild And Process Reset

Verdict: PASS / IIIE.6 UNBLOCKED; ZGAMMA PAPER-FIDELITY HOLD CARRIED. This slice implements the remesh-side duplicate/re-index/re-compact topology rebuild, process-state reset, and current-state `CollisionPending` filter wiring. It does not implement a full production remesh cadence, optimize replay, solve unresolved multi-hit policy, or replace the IIIE.4 zGamma placeholder.

## Scope

- IIIE.5 consumes fixture-owned per-global-vertex remesh assignment records, then partitions global TDS triangles into rebuilt plate-local triangulations; the future production cadence must supply these records from IIIE.3/IIIE.4 selection and gap-fill outputs.
- All-same triangles are copied to that plate. Two-of-three mixed triangles use the IIIE.1 named majority lab policy. One-one-one triple-junction triangles remain unresolved stop-condition anomalies.
- Plate-local topology is rebuilt by duplicate/re-index/re-compact from the global TDS assignment; no prior global owner, projection owner, centroid/random winner, or Stage 1.5 recovery path participates.
- Remesh reset clears active convergence lists, distance-to-front records, subduction matrix state, true subduction marks, obduction-pending marks, and collision-pending keys, then advances the reset serial.
- Plate geodetic motion is preserved byte-for-byte across topology rebuild. Generated IIIE.4 oceanic fields and q1/q2/qGamma event provenance are preserved in the remesh records, while `bPaperFaithfulZGammaProfile = false` remains a hold.
- The focused CollisionPending gate may use a fixture-owned accepted IIID2 group record when the compact live detector setup produces no accepted group; it tests mapping from accepted group records into current-state ray invisibility, not the IIID detector itself.

## Gates

| Gate | Result | Evidence |
|---|---:|---|
| Single-plate duplicate/re-index/re-compact | pass | applied `1`, samples `48`, global/assigned triangles `92/92`, all-same/majority/triple `92/0/0`, compact `1`, duplicate-authority `1`, fixture assignments `1`, motion `fc0b65aab6baf337/fc0b65aab6baf337`, reset serial `0->1`, active `0->0`, matrix `0->0`, pending collision `0->0`, generated/preserved `0/0`, policy/prior/projection `0/0/0`, q/zGamma `1/1`, hash `2b37d0b3a7afcc9b`. |
| Mixed triangle majority assignment | pass | applied `1`, samples `48`, global/assigned triangles `92/92`, all-same/majority/triple `86/6/0`, compact `1`, duplicate-authority `1`, fixture assignments `1`, motion `fb352eafaff07806/fb352eafaff07806`, reset serial `0->1`, active `35->0`, matrix `0->0`, pending collision `0->0`, generated/preserved `0/0`, policy/prior/projection `0/0/0`, q/zGamma `1/1`, hash `f5e9c76b88c01d88`. |
| Triple-junction unresolved anomaly | hold | applied `0`, samples `48`, global/assigned triangles `92/90`, all-same/majority/triple `82/8/2`, compact `1`, duplicate-authority `1`, fixture assignments `1`, motion `fb352eafaff07806/fb352eafaff07806`, reset serial `0->1`, active `35->0`, matrix `0->0`, pending collision `0->0`, generated/preserved `0/0`, policy/prior/projection `0/0/0`, q/zGamma `1/1`, hash `575458ebc333cfd4`. |
| Process-state reset at remesh | pass | applied `1`, samples `48`, global/assigned triangles `92/92`, all-same/majority/triple `92/0/0`, compact `1`, duplicate-authority `1`, fixture assignments `1`, motion `7a543f62177386b1/7a543f62177386b1`, reset serial `0->1`, active `25->0`, matrix `1->0`, pending collision `1->0`, generated/preserved `0/0`, policy/prior/projection `0/0/0`, q/zGamma `1/1`, hash `fd2305a108163d61`. |
| IIIE.4 provenance preservation | pass | applied `1`, samples `48`, global/assigned triangles `92/92`, all-same/majority/triple `92/0/0`, compact `1`, duplicate-authority `1`, fixture assignments `1`, motion `7a543f62177386b1/7a543f62177386b1`, reset serial `0->1`, active `24->0`, matrix `0->0`, pending collision `0->0`, generated/preserved `1/1`, policy/prior/projection `0/0/0`, q/zGamma `1/1`, hash `9c1011a383ae0420`. |
| CollisionPending current-state wiring | pass | accepted groups `1`, fixture-owned accepted group `1`, pending triangle keys `1`, filters sub/obd/coll `0/0/2`, policy/prior `0/0`, grouping `61e26f7c77aade0c`, selection `a6d6ae2e3727d152`. |
| Same-seed topology replay | pass | Replay topology hashes `f5e9c76b88c01d88` and `f5e9c76b88c01d88`. |
| Inherited IIIB independent signature regression | pass | `CarrierLabPhaseIIID7` regression artifact remains the state-consuming signature token: computed/expected `bf8818a26ed7b1dc`. IIIE.5 adds reset gates rather than rerunning the expensive integrated signature in this focused slice. |
| zGamma paper-fidelity hold | hold | IIIE.4 generated records are preserved with `bUsedZGammaDistanceProfilePlaceholder=1` and `bPaperFaithfulZGammaProfile=0`; topology may preserve them, but no report may claim the ridge-profile law is paper-faithful yet. |

## Contract Table

| Paper / IIIE.1 requirement | CarrierLab support now | Remaining obligation | Gate |
|---|---|---|---|
| Rebuild plate-local topology from global TDS vertex assignments | IIIE.5 duplicates, re-indexes, and compacts local vertices/triangles from fixture-owned assignment records | Wire this helper into the future production remesh cadence so live selection/gap-fill records supply the assignments | Compact topology and duplicate-authority gates |
| Mixed global-TDS triangles need explicit policy | All-same is direct; two-of-three majority is named lab policy; triple junction is a hold | Decide triple-junction handling only with paper citation or approved lab policy | Majority fixture and triple-junction hold fixture |
| Preserve plate geodetic motion across remesh | Motion hash before/after remains identical | Keep later remesh cadence from recomputing motion authority from projection | Motion hash gate |
| Reset process state at remesh | Active lists, distances, matrix state, subducting marks, obduction marks, and collision-pending keys reset to empty; reset serial advances | Later IIIB tracking must explicitly repopulate from geometry | Process reset fixture |
| Preserve divergent gap provenance | IIIE.4 q1/q2/qGamma, generated fields, and zGamma hold flags survive topology rebuild records | Full remesh event must attach these records per generated vertex | Oceanic provenance fixture |
| Accepted-but-unsutured collision groups are invisible to rays | Accepted IIID2 collision-group records seed `ConvergenceCollisionPendingTriangleKeys`, and IIIE.3 current-state selection maps them to `CollisionPending`; the compact fixture discloses whether it used a fixture-owned accepted record | Keep this wiring before any production remesh source selection | CollisionPending current-state wiring gate |

## Forbidden Policy Checks

| Forbidden policy | IIIE.5 status |
|---|---|
| Prior global sample owner/fraction fallback | Explicit per-record counter stays zero. |
| Projection-derived ownership authority | Explicit per-record counter stays zero. |
| Centroid/random/synthetic winner policy | Explicit per-record counter stays zero. |
| Stage 1.5 recovery/backfill/retention/hysteresis/anchoring | Not called; IIIE.5 uses a dedicated rebuild path rather than `RebuildPlateLocalStateFromSamples`. |
| Silent unresolved multi-hit routing | Explicit counter stays zero; triple-junction topology anomalies are holds, not winners. |
| zGamma paper-fidelity overclaim | Hold flag remains visible through generated records. |

## Stop Conditions For IIIE.6+

- Stop if a production remesh event calls the Stage 1.5 prior-owner/projection fallback path as primary IIIE authority.
- Stop if one-one-one triple-junction triangles are assigned without paper citation or explicit approved lab policy.
- Stop if active convergence lists, distance-to-front records, subduction matrix state, subducting marks, obduction marks, or collision-pending keys remain non-empty immediately after remesh reset.
- Stop if plate motion hashes change during topology rebuild.
- Stop if IIIE.4 generated vertices lose q1/q2/qGamma, signed velocity, age, elevation, ridge direction, or zGamma hold evidence during duplicate/re-index/re-compact.
- Stop if reports claim paper-faithful zGamma while generated records still report `bPaperFaithfulZGammaProfile = false`.

## Next Slice Boundary

IIIE.6 should be the remesh ledger/reframe slice: connect selection, divergent field generation, topology rebuild, and reset records into an event-level audit without adding optimization, new q1/q2 policy, ridge-profile replacement, rifting, erosion, or long-horizon validation. The ridge-profile law replacement remains owed before IIIH.0.

Metrics: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIE5/phase-iii-slice-iiie5-metrics.jsonl`.
