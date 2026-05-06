# Phase III Pre-IIIE.7 Report - D7 Input Pipeline Reuse

Status: MEASURED / PERFORMANCE HOT ROW REDUCED. This checkpoint re-runs the 60k Slice 5.5 single-hit source-triangle subdivision with IIID collision handling active after adding a component-wide cache for the read-only D3 destination-component expansion inside the D7 input pipeline. It does not add paper remeshing, qGamma oceanic generation, rifting, erosion, terrain displacement, ownership recovery, or projection repair.

Validation tier: `Integrated`. Integrated tier runs the 60k/40/32 path and is mandatory at sub-phase consolidation.

Output root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIID8/20260506T181544Z`

## Performance Result

Pre-IIIE.6 measured the IIID-active replay at `228.546042s` total and `217.860991s` collision-probe time, with D7 input pipeline consuming `214.184248s`. The first Pre-IIIE.7 exact-seed cache attempt was ineffective (`259.645839s` total, `246.529142s` input pipeline) because only `176` exact-seed cache hits occurred against `4148` destination-component builds.

The landed component-wide cache maps every triangle in a built destination component back to the cached traversal. That reduced the integrated IIID-active replay to `34.807920s` total and `25.282365s` collision-probe time. D7 input pipeline dropped to `21.924774s`, and D3 destination-component expansion dropped to `5.049106s` with `116` component builds and `4208` cache hits.

Paper Table 2 total tectonic-process baseline remains `0.190000s/step`. This run is `1.087748s/step`, or `5.72x` paper baseline for this investigation fixture. That is inside the `<=10x` soft target, but the scientific IIID.8 Slice 5.5 reduction gate still fails and remains an investigation finding.

## Gate Summary

| Gate | Result | Evidence |
|---|---:|---|
| Slice 5.5 fixed-fixture regression | pass | state `3b4a85366dab80db` / `3b4a85366dab80db`, ledger `bc3077100ba291b4` / `bc3077100ba291b4` |
| IIIB independent signature regression | pass | replay A `bf8818a26ed7b1dc`, replay B `bf8818a26ed7b1dc`, expected `bf8818a26ed7b1dc` |
| Baseline primary replay stable | pass | replay A `1936a14fde702261`, replay B `1936a14fde702261`, ledger `bc3077100ba291b4` / `bc3077100ba291b4` |
| Uniform-oceanic single-hit loss reduction | fail | baseline -0.518781666863, IIID -0.594389330059, reduction -14.57%, target 80.00% |
| Collision-transfer attribution | fail | eliminated -0.075607663196, transferred 6.340432106644, attribution -8385.96%, target 50.00% |
| No lab multi-hit policy influence | pass | baseline 0 / 0, IIID 0 / 0 |

| IIID active replay 1 | SKIP | Context-aware skip: replay 0 already failed or this path is investigation-only. Pass/fail remains false unless replay 1 runs and stability is proven. |

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
| iiid_active_primary_60k | 0 | 32 | 10 | 25.282365 | 13.826452 | 11.455913 | 0.790073894 | 1.382645180 | D1=32 D2=0 D3=0 D4=0 D5=0 D6=10 D7p=0 D7a=32 |
| iiid_active_primary_60k | 1 | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP |

## D1 Detection Split

| Fixture | Replay | D1 calls | Measured D1 seconds | Decision index | Hit sort | Hit classification | Component expansion | Inner-sea scan | Record construction | Audit hash |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| iiid_active_primary_60k | 0 | 32 | 16.488075 | 0.000184 | 0.003028 | 0.002541 | 14.963285 | 0.005513 | 1.513514 | 0.000009 |
| iiid_active_primary_60k | 1 | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP |

## D1 Detection Counts

| Fixture | Replay | Sorted hits | Collision candidates | Component builds | Component cache hits | Expanded continental triangles | Scanned oceanic triangles | Inner-sea triangles | Records |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| iiid_active_primary_60k | 0 | 90849 | 13788 | 380 | 13407 | 1067786 | 82 | 0 | 13787 |
| iiid_active_primary_60k | 1 | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP |

## D7 Apply Split

| Fixture | Replay | D7 apply calls | Total seconds | Input pipeline | Uplift plan | Apply-from-plan | Topology mutation | Record apply | Projection refresh | Hash |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| iiid_active_primary_60k | 0 | 32 | 25.281451 | 21.924774 | 1.770615 | 1.586044 | 0.821333 | 0.000266 | 0.764018 | 0.000168 |
| iiid_active_primary_60k | 1 | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP |

## D7 Input Pipeline Split

| Fixture | Replay | Input pipeline | Measured stage subtotal | D1 measured | D2 grouping | D3 destination mass | D4 slab-break | D5 suture | D3 destination component | D3 component builds | D3 cache hits |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| iiid_active_primary_60k | 0 | 21.924774 | 21.862459 | 16.488075 | 0.089547 | 5.106231 | 0.104520 | 0.074086 | 5.049106 | 116 | 4208 |
| iiid_active_primary_60k | 1 | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP |

## D7 Apply Counts

| Fixture | Replay | Planned records | Applied records | Applied topology mutations | No-uplift attempts |
|---|---:|---:|---:|---:|---:|
| iiid_active_primary_60k | 0 | 22135 | 22135 | 10 | 22 |
| iiid_active_primary_60k | 1 | SKIP | SKIP | SKIP | SKIP |

## Interpretation

The second IIID-active replay was not run because replay 0 is already an investigation result or the operator explicitly selected that policy. This report can still serve as an investigation checkpoint when replay 0 fails the quantitative gate, but it cannot serve as a passing determinism checkpoint. Consolidation gates expected to pass run replay 1 by default.

The IIID-active row does not meet one or both quantitative exit gates. Per the slice plan, this is an investigation checkpoint rather than permission to claim IIID closed the Slice 5.5 asymmetry. Do not use this report to claim full carrier/remesh success.

This is still a Phase II filtered-resampling comparison, not the IIIE paper remesh. Stage 1.5 remains foundation characterization; IIIE owns subducting/colliding-triangle remesh filtering, continuous q1/q2, qGamma oceanic generation, and process-state reset.

## Verdict

PERFORMANCE CHECKPOINT ACCEPTED. Pre-IIIE.7 reduces the integrated IIID-active replay from the prior `228.546042s` measurement to `34.807920s`, moving this investigation fixture from `37.59x` paper baseline to `5.72x` paper baseline. The D7 input-pipeline cache is diagnostics/read-only reuse of component traversal results and the focused IIID.7 equivalence report demonstrates cached and uncached D3/D4/D5 hashes match.

SCIENTIFIC IIID.8 STILL FAILS. The uniform-oceanic single-hit loss worsens from `-0.518781666863` to `-0.594389330059`, so this remains evidence that the old Phase II filtered-resampling comparison is not the right closure target for Slice 5.5. Do not use this report to claim IIID solved Stage 1.5, Slice 5.5, or the thesis remesh contract. Proceeding to IIIE.1 remains justified because IIIE.1 is the no-code remesh-contract audit that owns that failed closure target.
