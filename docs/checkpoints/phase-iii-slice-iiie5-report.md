# Phase IIIE.5 Topology Rebuild And Process Reset

Verdict: PASS / IIIE.6 UNBLOCKED; ZGAMMA PAPER-FIDELITY HOLD CARRIED. This slice implements the remesh-side duplicate/re-index/re-compact topology rebuild, process-state reset, and current-state `CollisionPending` filter wiring. It does not implement a full production remesh cadence, optimize replay, solve unresolved multi-hit policy, or claim the geophysics-derived IIIE.4 zGamma extension is paper-faithful.

## Scope

- IIIE.5 consumes fixture-owned per-global-vertex remesh assignment records, then partitions global TDS triangles into rebuilt plate-local triangulations; the future production cadence must supply these records from IIIE.3/IIIE.4 selection and gap-fill outputs.
- All-same triangles are copied to that plate. Two-of-three mixed triangles use the approved CarrierLab majority lab policy: when exactly two global TDS vertices assign to the same plate, that plate owns the rebuilt local triangle. This policy is deterministic, does not consult prior owner/projection state, and must be disclosed as lab policy rather than paper text.
- One-one-one triple-junction triangles use an approved CarrierLab centroid-split lab policy: the global triangle receives no whole-triangle winner, and each incident plate receives two boundary wedge triangles built from its original vertex, edge midpoints, and the spherical triangle centroid. Synthetic split vertices carry interpolated fields but no global sample ownership.
- Plate-local topology is rebuilt by duplicate/re-index/re-compact from the global TDS assignment; no prior global owner, projection owner, arbitrary winner, or Stage 1.5 recovery path participates.
- Remesh reset clears active convergence lists, distance-to-front records, subduction matrix state, true subduction marks, obduction-pending marks, and collision-pending keys, then advances the reset serial.
- Plate geodetic motion is preserved byte-for-byte across topology rebuild. Generated IIIE.4 oceanic fields and q1/q2/qGamma event provenance are preserved in the remesh records, while `bPaperFaithfulZGammaProfile = false` remains a hold.
- The focused CollisionPending gate may use a fixture-owned accepted IIID2 group record when the compact live detector setup produces no accepted group; it tests mapping from accepted group records into current-state ray invisibility, not the IIID detector itself.

## Gates

| Gate | Result | Evidence |
|---|---:|---|
| Single-plate duplicate/re-index/re-compact | pass | applied `1`, samples `48`, global/assigned triangles `92/92`, all-same/majority/split/unresolved `92/0/0/0`, split local/synth `0/0`, compact `1`, duplicate-authority `1`, fixture assignments `1`, motion `fc0b65aab6baf337/fc0b65aab6baf337`, reset serial `0->1`, active `0->0`, matrix `0->0`, pending collision `0->0`, generated/preserved `0/0`, policy/prior/projection `0/0/0`, q/zGamma `1/1`, hash `c32f6b4296715830`. |
| Mixed triangle majority assignment | pass | applied `1`, samples `48`, global/assigned triangles `92/92`, all-same/majority/split/unresolved `86/6/0/0`, split local/synth `0/0`, compact `1`, duplicate-authority `1`, fixture assignments `1`, motion `fb352eafaff07806/fb352eafaff07806`, reset serial `0->1`, active `35->0`, matrix `0->0`, pending collision `0->0`, generated/preserved `0/0`, policy/prior/projection `0/0/0`, q/zGamma `1/1`, hash `5ae9abddc4feb5b4`. |
| Triple-junction centroid split | pass | applied `1`, samples `48`, global/assigned triangles `92/92`, all-same/majority/split/unresolved `82/8/2/0`, split local/synth `12/18`, compact `1`, duplicate-authority `1`, fixture assignments `1`, motion `fb352eafaff07806/fb352eafaff07806`, reset serial `0->1`, active `35->0`, matrix `0->0`, pending collision `0->0`, generated/preserved `0/0`, policy/prior/projection `0/0/0`, q/zGamma `1/1`, hash `111227a118ccc36d`. |
| Process-state reset at remesh | pass | applied `1`, samples `48`, global/assigned triangles `92/92`, all-same/majority/split/unresolved `92/0/0/0`, split local/synth `0/0`, compact `1`, duplicate-authority `1`, fixture assignments `1`, motion `7a543f62177386b1/7a543f62177386b1`, reset serial `0->1`, active `25->0`, matrix `1->0`, pending collision `1->0`, generated/preserved `0/0`, policy/prior/projection `0/0/0`, q/zGamma `1/1`, hash `dc6049722b620d5e`. |
| IIIE.4 provenance preservation | pass | applied `1`, samples `48`, global/assigned triangles `92/92`, all-same/majority/split/unresolved `92/0/0/0`, split local/synth `0/0`, compact `1`, duplicate-authority `1`, fixture assignments `1`, motion `7a543f62177386b1/7a543f62177386b1`, reset serial `0->1`, active `24->0`, matrix `0->0`, pending collision `0->0`, generated/preserved `1/1`, policy/prior/projection `0/0/0`, q/zGamma `1/1`, hash `03dd6859896f836b`. |
| CollisionPending current-state wiring | pass | accepted groups `1`, fixture-owned accepted group `1`, pending triangle keys `1`, filters sub/obd/coll `0/0/2`, policy/prior `0/0`, grouping `61e26f7c77aade0c`, selection `a6d6ae2e3727d152`. |
| Same-seed topology replay | pass | Replay topology hashes `5ae9abddc4feb5b4` and `5ae9abddc4feb5b4`. |
| Inherited IIIB independent signature regression | pass | `CarrierLabPhaseIIID7` regression artifact remains the state-consuming signature token: computed/expected `bf8818a26ed7b1dc`. IIIE.5 adds reset gates rather than rerunning the expensive integrated signature in this focused slice. |
| zGamma paper-fidelity hold | hold | IIIE.4 generated records are preserved with `bUsedZGammaDistanceProfilePlaceholder=0`, `bUsedZGammaGeophysicsDerivedProfile=1`, and `bPaperFaithfulZGammaProfile=0`; topology may preserve them, but no report may claim the ridge-profile law is paper-faithful. |

## Contract Table

| Paper / IIIE.1 requirement | CarrierLab support now | Remaining obligation | Gate |
|---|---|---|---|
| Rebuild plate-local topology from global TDS vertex assignments | IIIE.5 duplicates, re-indexes, and compacts local vertices/triangles from fixture-owned assignment records | Wire this helper into the future production remesh cadence so live selection/gap-fill records supply the assignments | Compact topology and duplicate-authority gates |
| Mixed global-TDS triangles need explicit policy | All-same is direct; two-of-three majority and one-one-one centroid split are approved CarrierLab lab policies | Consolidation must disclose both policies as lab extensions, not paper text | Majority fixture and centroid-split fixture |
| Preserve plate geodetic motion across remesh | Motion hash before/after remains identical | Keep later remesh cadence from recomputing motion authority from projection | Motion hash gate |
| Reset process state at remesh | Active lists, distances, matrix state, subducting marks, obduction marks, and collision-pending keys reset to empty; reset serial advances | Later IIIB tracking must explicitly repopulate from geometry | Process reset fixture |
| Preserve divergent gap provenance | IIIE.4 q1/q2/qGamma, generated fields, and zGamma hold flags survive topology rebuild records | Full remesh event must attach these records per generated vertex | Oceanic provenance fixture |
| Accepted-but-unsutured collision groups are invisible to rays | Accepted IIID2 collision-group records seed `ConvergenceCollisionPendingTriangleKeys`, and IIIE.3 current-state selection maps them to `CollisionPending`; the compact fixture discloses whether it used a fixture-owned accepted record | Keep this wiring before any production remesh source selection | CollisionPending current-state wiring gate |

## Forbidden Policy Checks

| Forbidden policy | IIIE.5 status |
|---|---|
| Prior global sample owner/fraction fallback | Explicit per-record counter stays zero. |
| Projection-derived ownership authority | Explicit per-record counter stays zero. |
| Uncited remesh winner policy | Explicit per-record counter stays zero. |
| Stage 1.5 recovery/backfill/retention/hysteresis/anchoring | Not called; IIIE.5 uses a dedicated rebuild path rather than `RebuildPlateLocalStateFromSamples`. |
| Silent unresolved multi-hit routing | Explicit counter stays zero; triple-junction topology uses centroid subdivision, not a ray multi-hit winner. |
| zGamma paper-fidelity overclaim | Hold flag remains visible through generated records. |

## Approved Lab Policy

Two-of-three mixed global-TDS triangles are approved for IIIE live-cadence use as a narrow CarrierLab lab policy. The rule is only valid when exactly two vertices have the same post-remesh assigned plate and the third differs; the majority plate receives the rebuilt triangle. One-one-one mixed global-TDS triangles are separately approved for IIIE live-cadence use as centroid-split topology: each incident plate receives two local boundary wedge triangles and the global triangle receives no single authoritative winner. These approvals do not extend to unresolved ray multi-hits, prior-owner fallback, projection-derived ownership, or arbitrary winner policies.

## Stop Conditions For IIIE.6+

- Stop if a production remesh event calls the Stage 1.5 prior-owner/projection fallback path as primary IIIE authority.
- Stop if one-one-one triple-junction triangles receive a whole-triangle winner instead of centroid-split subdivision or an explicit future policy.
- Stop if active convergence lists, distance-to-front records, subduction matrix state, subducting marks, obduction marks, or collision-pending keys remain non-empty immediately after remesh reset.
- Stop if plate motion hashes change during topology rebuild.
- Stop if IIIE.4 generated vertices lose q1/q2/qGamma, signed velocity, age, elevation, ridge direction, or zGamma hold evidence during duplicate/re-index/re-compact.
- Stop if reports claim paper-faithful zGamma while generated records still report `bUsedZGammaGeophysicsDerivedProfile = true` and `bPaperFaithfulZGammaProfile = false`.

## Next Slice Boundary

IIIE.6 should be the remesh ledger/reframe slice: connect selection, divergent field generation, topology rebuild, and reset records into an event-level audit without adding optimization, new q1/q2 policy, further ridge-profile replacement, rifting, erosion, or long-horizon validation. The zGamma profile remains a geophysics-derived lab extension unless a paper-cited closed form is recovered.

Metrics: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIE5/phase-iii-slice-iiie5-metrics.jsonl`.
