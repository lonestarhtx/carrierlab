# Phase III Pre-IIID.3 Audit Triage

Status: hardening tranche before IIID.3. IIID.1 and IIID.2 remain accepted read-only collision slices; this checkpoint blocks only IIID.3 topology mutation until the proof-strength and live-path concerns below are addressed.

## Source Check

Direct thesis page-image inspection for slab pull used `docs/Synthèse de terrain à léchelle planétaire/cc5c6807-074.png` (thesis page 63). The source text says the implementation updates both the axis and speed of `G_i` for slab pull. Therefore the audit's axis-only slab-pull reading is rejected; the hardening keeps the current omega-vector axis+speed update and strengthens the oracle instead.

## Hardening Applied Now

- IIIC.4 and IIIC consolidation no longer use enabled-run slab-pull contribution records as their only oracle source. The expected slab-pull audit is computed from a matching process-layer replay with slab pull disabled, after the same subducting marks are produced, using that mirror state, plate vertices, motion vectors, marks, and configured constants.
- Standalone IIIB consolidation now compares the computed local-vs-pair independent signature directly to `bf8818a26ed7b1dc`. The no-admissible negative signature remains diagnostic because it is intentionally a different fixture.
- Standalone IIIC.4 and IIIC.5 rows that only check closure recomputation are explicitly labeled non-gating smoke/continuity, not IIIB independent signature gates.
- Manual `ApplyResampleEvent()` and editor `Lab Resample Now` are labeled as the Stage 1.5 lab-policy remesh path until IIIE. Actor metrics expose `LastRemeshMode` and `PolicyResolvedMultiHitCount`.
- IIID.1 and IIID.2 commandlets now gate their collision evidence on `PolicyResolvedMultiHitCount == 0`, so centroid/random/synthetic multi-hit policy cannot influence accepted read-only collision records.
- Stage 1.5 framing is narrowed: pre-remesh miss/overlap accumulation is a rigid-window projection finding; remesh/material preservation is the part that cannot claim thesis alignment without Phase III process state.

## Finding Triage

| Audit finding | Classification | Disposition |
|---|---|---|
| P0-1 IIIC.4 slab-pull oracle is circular | Fixed now | Replaced enabled-run contribution-record oracle with disabled-mirror-state expected audit in IIIC.4 and IIIC consolidation. |
| P0-2 Stage 1.5 reframing diagnoses too much | Fixed now | Docs split rigid-window miss/overlap evidence from remesh/material-preservation evidence. Stage 1.5 hard-gate failure remains visible. |
| P0-3 Stage 1.5-style gap-fill runs on live viewer cadence path | Fixed now, scheduled before IIIE remesh | Live path is labeled as lab-policy remesh and exposes remesh mode. IIIE must replace this as the paper-primary remesh path. |
| P0-4 `MultiHitPolicy = Centroid` is live default and used in IIID fixtures | Fixed now, scheduled before IIIE remesh | IIID.1/IIID.2 now fail if policy-resolved multi-hit count is nonzero. IIIE must retire policy tiebreaks from the paper-primary remesh path. |
| P0-5 IIIB expected signature enforced only inside IIIC consolidation | Fixed now | Standalone IIIB consolidation directly compares the local-vs-pair independent signature to `bf8818a26ed7b1dc`. |
| P1-1 Continental-continental obduction profile unimplemented/doc overclaim | Fixed before IIIE.1 | Pre-IIIE.8 adds a separate obduction-pending mark/audit path for `CollisionCandidate` contacts and gates deterministic pre-threshold continuous uplift without topology mutation. |
| P1-2 Fold-direction convergence increment unimplemented | Fixed before IIIE.1 | The pre-IIIE thesis cleanup tranche corrected IIIC.3 fold-direction updates to use the raw tangent relative velocity step scaled by `PhaseIIICFoldDirectionBeta`, with an independent commandlet oracle. |
| P1-3 Slab pull updates omega as axis*speed vector | Rejected with evidence | Thesis page image says both axis and speed are updated. Keep behavior; proof-strength issue is handled by oracle hardening. |
| P1-4 Equal-age ocean-ocean polarity deferred silently | Scheduled before IIIH | Needs long-horizon/deferred-count gate or earlier check if polarity code is touched. |
| P1-5 No-admissible negative uses synthetic evidence | Scheduled before IIIH | Remains acceptable as downstream diagnostic because reports state the limitation. A natural-geometry negative should be added before IIIH if feasible. |
| P1-6 Distance-to-front neighbor seeding interpretation undocumented | Scheduled before IIIH | Add source quote or explicit lab-policy note before long-horizon equilibrium claims. |
| P2-1 "IIID solves continental loss" can become unfalsifiable | Scheduled before IIIH | IIIH must make the reduction gate falsifiable and compare alternatives instead of treating investigation as success. |
| P2-2 Slice 5.5 bypass validates known-imperfect baseline | Fixed now | Reports and triage label it as fixed-fixture regression, not proof of global paper faithfulness. |
| P2-3 IIIE.1 audit alone may retain lab policy code | Scheduled before IIIE remesh | IIIE must make the paper-primary path fail loudly on unresolved multi-hit after filtering; lab policies may remain only as comparison controls. |
| P2-4 Two-plate fixtures do not exercise multi-pair/triple-junction sparsity | Scheduled before IIIH | Add multi-pair/triple-junction fixtures before global equilibrium claims. |
| P2-5 `g(v)` clamp differs from linear thesis reading | Scheduled before IIIH | Source-check when velocity transfer is revisited; not live for this tranche because slab pull remains capped at `v0`. |
| P3-1 `AGENTS.md` still contains older stage precondition wording | Scheduled before IIIH docs hygiene | Treat as stale repository guidance relative to accepted Phase III checkpoints; clean after this hardening tranche if it keeps confusing reviews. |
| P3-2 "no baseline available" can become a generic bypass label | Scheduled before IIIH claim audit | Future reports must say "fixed-fixture regression" or "no comparable baseline" with scope; not proof. |
| P3-3 Superseded docs remain cited together | Scheduled before IIIE remesh | Before IIIE implementation, update citation hierarchy so `paper-resampling-extraction.md` is canonical for remesh. |
| P3-4 Visible trench depth hardcoded at `zt = -10 km` | Scheduled before IIID collision uplift | Source-check trench value/provenance before uplift/event reports claim full elevation process fidelity. |

## Stop Conditions Before IIID.3

- IIIC.4 and IIIC consolidated reports must show independent slab-pull oracle comparison from disabled mirror state with residuals at floating-point tolerance.
- Any report row named as a gate must feed the final commandlet pass/fail path.
- IIID.1 and IIID.2 must pass with `PolicyResolvedMultiHitCount == 0` for every collision fixture.
- Live/manual remesh must be labeled as Stage 1.5 lab policy until IIIE replaces it.
- No report may claim Stage 1.5 was retroactively solved.

## Remesh Guard Scan Classification

The hardening scan intentionally still finds high-severity heuristic hits in `CarrierLabVisualizationActor.cpp` and the editor panel because Stage 1.5/lab-policy remesh controls, multi-hit policy controls, and Phase II comparison/filter ledgers still exist. These are classified as `diagnostic/control only` for this tranche. They are not promoted to the paper-primary remesh path, and IIID.1/IIID.2 now fail if policy-resolved multi-hit selection influences their collision evidence.

The remaining primary-remesh risk is therefore scheduled, not accepted: IIIE must implement the §3.3.2.3 paper-remesh path with subducting/colliding triangle filtering, continuous q1/q2, qGamma provenance, fail-loud unresolved multi-hit handling, and process-state reset. Until IIIE lands, `ApplyResampleEvent()` and manual `Lab Resample Now` remain labeled Stage 1.5/lab-policy output and must not be used as paper-primary evidence.

## Pre-IIIE Cleanup Addendum

The follow-on pre-IIIE cleanup tranche closes the three convergence-side audit findings that were left open after IIID:

- Finding 9 is fixed by `docs/checkpoints/phase-iii-pre-iiie-thesis-cleanup.md` and the regenerated IIIC.3 report: fold direction now follows the thesis `beta * (s_i - s_j)` form rather than scaling by uplift magnitude.
- Finding 20 is fixed by `docs/checkpoints/phase-iii-pre-iiie-thesis-cleanup.md` and the regenerated IIID.7 report: collision uplift radius now uses `r_c * sqrt(v/v_0) * A/A_0`.
- Finding 33 is fixed by `docs/checkpoints/phase-iii-pre-iiie8-obduction-bridge.md`: continental-continental `CollisionCandidate` evidence now produces separate obduction marks and continuous uplift before the 300 km collision threshold, while event count and plate-local triangle count remain unchanged.

## IIID.3 Unblock Criteria

IIID.3 may begin after the hardening commandlets pass, reports are regenerated, and this triage document is committed. IIID.3 remains scoped to collision topology work. It must not claim thesis remesh alignment, rifting, erosion, terrain morphology, or standalone Stage 1.5 success.
