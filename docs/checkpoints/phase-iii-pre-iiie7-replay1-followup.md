# Phase III Slice IIID.8 Report - Slice 5.5 Asymmetry Recheck

Status: FAIL as a scientific IIID.8 asymmetry-reduction gate; PASS as the Pre-IIIE.7 integrated replay-1 determinism follow-up. This slice re-runs the 60k Slice 5.5 single-hit source-triangle subdivision with IIID collision handling active, then compares the uniform-oceanic single-hit continental loss against the accepted Phase II Slice 5.5 baseline. It does not add paper remeshing, qGamma oceanic generation, rifting, erosion, terrain displacement, ownership recovery, or projection repair.

Follow-up disposition: replay 0 and replay 1 are byte-identical for both the baseline and IIID-active integrated rows, including the active replay hash `6a0e09990609552b`, ledger `17159143d4c10602`, 10 collision events, zero policy-resolved multi-hits, and uniform-oceanic net `-0.594389330059`. The quantitative IIID.8 gate remains failed and remains handed to IIIE; this report must not be used to claim IIID.8 success.

Validation tier: `Integrated`. Integrated tier runs the 60k/40/32 path and is mandatory at sub-phase consolidation.

Output root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIID8/20260506T184307Z`

## Gate Summary

| Gate | Result | Evidence |
|---|---:|---|
| Slice 5.5 fixed-fixture regression | pass | state `3b4a85366dab80db` / `3b4a85366dab80db`, ledger `bc3077100ba291b4` / `bc3077100ba291b4` |
| IIIB independent signature regression | pass | replay A `bf8818a26ed7b1dc`, replay B `bf8818a26ed7b1dc`, expected `bf8818a26ed7b1dc` |
| Baseline primary replay stable | pass | replay A `1936a14fde702261`, replay B `1936a14fde702261`, ledger `bc3077100ba291b4` / `bc3077100ba291b4` |
| Uniform-oceanic single-hit loss reduction | fail | baseline -0.518781666863, IIID -0.594389330059, reduction -14.57%, target 80.00% |
| Collision-transfer attribution | fail | eliminated -0.075607663196, transferred 6.340432106644, attribution -8385.96%, target 50.00% |
| No lab multi-hit policy influence | pass | baseline 0 / 0, IIID 0 / 0 |

## Primary Rows

| Fixture | Replay | IIID | Events | Uniform oceanic count/net | Uniform continental count/net | Mixed count/net | Auth CAF before/after | Ledger hash | Replay hash |
|---|---:|---:|---:|---:|---:|---:|---:|---|---|
| phase_ii_slice55_baseline_60k | 0 | no | 0 | 16742 / -0.518781666863 | 7057 / 0.148073733739 | 168 / -0.004269205869 | 0.301050000000 / 0.273075416991 | `bc3077100ba291b4` | `1936a14fde702261` |
| phase_ii_slice55_baseline_60k | 1 | no | 0 | 16742 / -0.518781666863 | 7057 / 0.148073733739 | 168 / -0.004269205869 | 0.301050000000 / 0.273075416991 | `bc3077100ba291b4` | `1936a14fde702261` |
| iiid_active_primary_60k | 0 | yes | 10 | 16040 / -0.594389330059 | 5543 / 0.138648955778 | 106 / -0.006497126656 | 0.301050000000 / 0.264153587545 | `17159143d4c10602` | `6a0e09990609552b` |
| iiid_active_primary_60k | 1 | yes | 10 | 16040 / -0.594389330059 | 5543 / 0.138648955778 | 106 / -0.006497126656 | 0.301050000000 / 0.264153587545 | `17159143d4c10602` | `6a0e09990609552b` |

## Integrated Collision Timing

| Fixture | Replay | Attempts | Events | Collision probe seconds | Mutation seconds | No-mutation seconds | Seconds / attempt | Seconds / event | Nested calls |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---|
| iiid_active_primary_60k | 0 | 32 | 10 | 25.265243 | 13.831055 | 11.434188 | 0.789538844 | 1.383105510 | D1=32 D2=0 D3=0 D4=0 D5=0 D6=10 D7p=0 D7a=32 |
| iiid_active_primary_60k | 1 | 32 | 10 | 25.184784 | 13.766701 | 11.418083 | 0.787024500 | 1.376670130 | D1=32 D2=0 D3=0 D4=0 D5=0 D6=10 D7p=0 D7a=32 |

## D1 Detection Split

| Fixture | Replay | D1 calls | Measured D1 seconds | Decision index | Hit sort | Hit classification | Component expansion | Inner-sea scan | Record construction | Audit hash |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| iiid_active_primary_60k | 0 | 32 | 16.476877 | 0.000183 | 0.003014 | 0.002778 | 14.941955 | 0.005546 | 1.523392 | 0.000009 |
| iiid_active_primary_60k | 1 | 32 | 16.438676 | 0.000183 | 0.002952 | 0.002781 | 14.911509 | 0.005382 | 1.515858 | 0.000011 |

## D1 Detection Counts

| Fixture | Replay | Sorted hits | Collision candidates | Component builds | Component cache hits | Expanded continental triangles | Scanned oceanic triangles | Inner-sea triangles | Records |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| iiid_active_primary_60k | 0 | 90849 | 13788 | 380 | 13407 | 1067786 | 82 | 0 | 13787 |
| iiid_active_primary_60k | 1 | 90849 | 13788 | 380 | 13407 | 1067786 | 82 | 0 | 13787 |

## D7 Apply Split

| Fixture | Replay | D7 apply calls | Total seconds | Input pipeline | Uplift plan | Apply-from-plan | Topology mutation | Record apply | Projection refresh | Hash |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| iiid_active_primary_60k | 0 | 32 | 25.264305 | 21.911201 | 1.773501 | 1.579582 | 0.817022 | 0.000275 | 0.761826 | 0.000174 |
| iiid_active_primary_60k | 1 | 32 | 25.183836 | 21.854734 | 1.761656 | 1.567430 | 0.815187 | 0.000275 | 0.751527 | 0.000172 |

## D7 Input Pipeline Split

| Fixture | Replay | Input pipeline | Measured stage subtotal | D1 measured | D2 grouping | D3 destination mass | D4 slab-break | D5 suture | D3 destination component | D3 component builds | D3 cache hits |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| iiid_active_primary_60k | 0 | 21.911201 | 21.848705 | 16.476877 | 0.089438 | 5.102099 | 0.105282 | 0.075008 | 5.045283 | 116 | 4208 |
| iiid_active_primary_60k | 1 | 21.854734 | 21.792632 | 16.438676 | 0.089086 | 5.086728 | 0.104329 | 0.073813 | 5.029986 | 116 | 4208 |

## D7 Apply Counts

| Fixture | Replay | Planned records | Applied records | Applied topology mutations | No-uplift attempts |
|---|---:|---:|---:|---:|---:|
| iiid_active_primary_60k | 0 | 22135 | 22135 | 10 | 22 |
| iiid_active_primary_60k | 1 | 22135 | 22135 | 10 | 22 |

## Interpretation

The IIID-active row does not meet one or both quantitative exit gates. Per the slice plan, this is an investigation checkpoint rather than permission to claim IIID closed the Slice 5.5 asymmetry. Do not use this report to claim full carrier/remesh success.

This is still a Phase II filtered-resampling comparison, not the IIIE paper remesh. Stage 1.5 remains foundation characterization; IIIE owns subducting/colliding-triangle remesh filtering, continuous q1/q2, qGamma oceanic generation, and process-state reset.

## Verdict

FAIL scientifically; replay-1 determinism follow-up complete. The failed quantitative gate remains a IIIE handoff item rather than permission to claim IIID.8 success.
