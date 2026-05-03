# Phase III Slice IIID.8 Report - Slice 5.5 Asymmetry Recheck

Status: FAIL / INVESTIGATION. This checkpoint rechecked the accepted Phase II Slice 5.5 uniform-oceanic source-triangle loss with IIID collision handling active. The completed IIID-active replay moved the headline metric in the wrong direction, so Phase IIID should not be consolidated as "finished" on the original IIID.8 success claim.

Source log: `Saved/Logs/CarrierLabPhaseIIID8.log`

Output root from interrupted run: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIID8/20260503T191717Z`

## Gate Summary

| Gate | Result | Evidence |
|---|---:|---|
| Slice 5.5 fixed baseline | PASS | Accepted Slice 5.5 baseline: state `3b4a85366dab80db`, material ledger `bc3077100ba291b4`, uniform-oceanic net `-0.518782`. |
| IIIB independent signature | PASS | Recomputed signatures from replay 0 and replay 1 both matched expected token `bf8818a26ed7b1dc` at step 51 before the IIID-active replay. |
| IIID-active replay 0 completion | PASS | 60k/40, 32 steps, 10 collision/suture mutations, transfer `6.340432106644`, completed in `1151.130s`. |
| IIID-active replay 1 | SKIP | Stopped by user request after replay 0 exposed the quantitative no-go. Passing IIID.8 still requires an explicit second replay. |
| Uniform-oceanic single-hit loss reduction | FAIL | Baseline `-0.518782`; IIID active replay 0 `-0.594389330059`; reduction `-14.57%` against the `>= 80%` target. |
| Collision-transfer attribution | FAIL | Collision transfer was positive (`6.340432106644`), but no loss was eliminated; the uniform-oceanic loss increased by `0.075607330059`. |

## Completed Replay 0 Evidence

| Metric | Value |
|---|---:|
| Samples | 60000 |
| Plates | 40 |
| Steps | 32 |
| Collision mutations | 10 |
| Transferred continental area | 6.340432106644 |
| Final uniform-oceanic net | -0.594389330059 |
| Baseline uniform-oceanic net | -0.518782 |
| Delta vs baseline | -0.075607330059 |
| Reduction fraction | -14.574% |
| Replay wall time | 1151.130s |

## Interpretation

IIID collision/suture is doing substantial topology work: ten collision events transferred continental area during the 32-step window. However, that work does not reduce the Slice 5.5 uniform-oceanic single-hit loss under the current Phase II filtered-resampling comparison. It makes the measured bucket worse.

This does not demonstrate that the IIID collision implementation is wrong. It demonstrates that the original IIID.8 success hypothesis was too strong for the current measurement path. The comparison still ends with the Phase II filtered resampling event, not the paper-faithful IIIE remesh. In other words: collision/suture can be working, while the old Slice 5.5 uniform-oceanic bucket remains the wrong closure metric until the remesh path filters subducting/colliding triangles and rebuilds plates according to the thesis contract.

## No-Go Decision

No-go for treating IIID.8 as passed.

No-go for Phase IIID consolidation as a completed sub-phase on the original ">= 80% reduction" headline.

Go for a narrow correction path: reframe IIID.8 as an investigation result, preserve IIID.1-IIID.7 as accepted implementation slices, and decide whether to close IIID with an explicit unresolved handoff to IIIE or add a replacement IIID.8 gate that measures collision/suture conservation directly rather than the old Phase II filtered-remesh bucket.

## Notes

- The second IIID-active replay was intentionally stopped by user request because replay 0 had already failed the quantitative gate and replay 1 would have been expensive.
- The commandlet now defaults to a single IIID-active replay. A full passing determinism run must opt in with `-RunActiveReplay1`.
- This checkpoint must not be used to claim Stage 1.5 is fixed, that full paper remeshing is implemented, or that Phase IIID closes the Slice 5.5 asymmetry.
