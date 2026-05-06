# Phase III Pre-IIIE.4 Report - Integrated Remeasurement

Status: MEASURED / PERFORMANCE FINDING REMAINS. This checkpoint re-runs the real IIID.8 60k/40/32 integrated path after the Pre-IIIE.3 collision plan reuse refactor. It measures cost against the paper Table 2 tectonic-process baseline and preserves the existing IIID.8 quantitative failure. It does not add paper remeshing, qGamma oceanic generation, rifting, erosion, terrain displacement, ownership recovery, projection repair, or any new carrier authority.

Validation tier: `Integrated`. Integrated tier runs the 60k sample / 40 plate / 32 step path and remains mandatory at sub-phase consolidation. Active replay 1 was intentionally skipped because active replay 0 still fails the IIID.8 investigation gate; this report is a timing and investigation checkpoint, not a passing determinism checkpoint.

Output root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIID8/20260506T031135Z`

## Paper Baseline Comparison

Paper Table 2 reports tectonic-process cost at 60k samples / 40 plates as `0.19s` per timestep. The old IIID.8 integrated replay 0 took `1151.130s` over 32 steps. The committed instrumented remeasurement took `264.030493s` over 32 steps.

| Run | Active replay 0 seconds | Seconds / step | Paper baseline | Ratio | Change vs old IIID.8 |
|---|---:|---:|---:|---:|---:|
| Old IIID.8 integrated replay 0 | 1151.130000 | 35.972813 | 0.190000 | 189.33x | baseline |
| Pre-IIIE.4 instrumented integrated replay 0 | 264.030493 | 8.250953 | 0.190000 | 43.43x | 77.06% faster |

One uninstrumented post-refactor probe completed active replay 0 in `211.760628s` (`6.617520s/step`, `34.83x` paper baseline), but this checkpoint uses the instrumented `264.030493s` run because it also records structural call-count evidence.

Interpretation: Pre-IIIE.3 materially reduced integrated collision cost, but the path remains far above the tracked `10x` paper-baseline target. The performance finding is narrowed, not closed.

## Gate Summary

| Gate | Result | Evidence |
|---|---:|---|
| Slice 5.5 fixed-fixture regression | pass | state `3b4a85366dab80db` / `3b4a85366dab80db`, ledger `bc3077100ba291b4` / `bc3077100ba291b4` |
| IIIB independent signature regression | pass | replay A `bf8818a26ed7b1dc`, replay B `bf8818a26ed7b1dc`, expected `bf8818a26ed7b1dc` |
| Baseline primary replay stable | pass | replay A `1936a14fde702261`, replay B `1936a14fde702261`, ledger `bc3077100ba291b4` / `bc3077100ba291b4` |
| Uniform-oceanic single-hit loss reduction | fail | baseline -0.518781666863, IIID -0.594389330059, reduction -14.57%, target 80.00% |
| Collision-transfer attribution | fail | eliminated -0.075607663196, transferred 6.340432106644, attribution -8385.96%, target 50.00% |
| No lab multi-hit policy influence | pass | baseline 0 / 0, IIID 0 / 0 |
| IIID active replay 1 | skip | Context-aware skip: active replay 0 already failed the investigation gate. This report does not claim active-run determinism. |

## Primary Rows

| Fixture | Replay | IIID | Events | Uniform oceanic count/net | Uniform continental count/net | Mixed count/net | Auth CAF before/after | Ledger hash | Replay hash |
|---|---:|---:|---:|---:|---:|---:|---:|---|---|
| phase_ii_slice55_baseline_60k | 0 | no | 0 | 16742 / -0.518781666863 | 7057 / 0.148073733739 | 168 / -0.004269205869 | 0.301050000000 / 0.273075416991 | `bc3077100ba291b4` | `1936a14fde702261` |
| phase_ii_slice55_baseline_60k | 1 | no | 0 | 16742 / -0.518781666863 | 7057 / 0.148073733739 | 168 / -0.004269205869 | 0.301050000000 / 0.273075416991 | `bc3077100ba291b4` | `1936a14fde702261` |
| iiid_active_primary_60k | 0 | yes | 10 | 16040 / -0.594389330059 | 5543 / 0.138648955778 | 106 / -0.006497126656 | 0.301050000000 / 0.264153587545 | `17159143d4c10602` | `6a0e09990609552b` |
| iiid_active_primary_60k | 1 | yes | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP |

## Integrated Collision Timing

| Fixture | Replay | Attempts | Events | Collision probe seconds | Mutation seconds | No-mutation seconds | Seconds / attempt | Seconds / event | Nested calls |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---|
| iiid_active_primary_60k | 0 | 32 | 10 | 254.605704 | 240.733618 | 13.872086 | 7.956428253 | 24.073361830 | D1=32 D2=0 D3=0 D4=0 D5=0 D6=10 D7p=0 D7a=32 |
| iiid_active_primary_60k | 1 | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP |

The structural call counts show the Pre-IIIE.3 plan-reuse refactor did collapse the nested IIID.2 through IIID.5 public commandlet-style calls inside active collision application. The remaining integrated cost is dominated by one IIID.1 terrane-detection pass per attempted timestep plus IIID.7 apply work, not repeated D2/D3/D4/D5 recomputation.

## Interpretation

This checkpoint answers the specific Pre-IIIE.4 question: the refactor survives integrated measurement and cuts the old 189x paper-baseline gap to 43x in the instrumented run. That is a large improvement, but it is still above the tracked 10x soft target and still far from paper-interactive behavior.

The IIID.8 quantitative gate still fails. Uniform-oceanic single-hit loss worsens relative to the Phase II Slice 5.5 baseline, and collision-transfer attribution is not meaningful under the old filtered-resampling comparison. This remains evidence that the old Slice 5.5 metric is not a closure metric for IIID; it is not evidence that Stage 1.5, Slice 5.5, or thesis remeshing has been solved.

The next performance target should be candidate/terrane detection cost on integrated runs, especially the D1-per-attempt pattern. Any optimization must remain diagnostics- and geometry-derived; it must not introduce cache-as-authority, persistent global ownership, repair, backfill, projection-history hints, or retained lab-policy remesh behavior.

## Verdict

MEASURED. Pre-IIIE.3 reduced integrated cost enough to retire the immediate "did Slice-tier numbers hide the real path" concern, but not enough to close the performance finding. Keep the performance yellow flag open and target D1/candidate-detection cost in the next performance slice.
