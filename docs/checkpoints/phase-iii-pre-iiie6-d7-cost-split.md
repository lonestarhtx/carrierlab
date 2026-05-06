# Phase III Pre-IIIE.6 Report - D7 Apply / Topology / Uplift Cost Split

Status: MEASURED / D7 APPLY DOMINATED BY INPUT PIPELINE. This checkpoint instruments the remaining D7 apply cost from Pre-IIIE.5 by splitting the public `ApplyPhaseIIID7CollisionUplift` path into diagnostics-only rows: repeated input pipeline, uplift planning, apply-from-plan, topology mutation, record application, projection refresh, and hash finalization.

This slice adds no new collision behavior, carrier authority, remesh behavior, ownership persistence, repair, recovery, backfill, projection-derived state, or cache-as-authority. The added fields are resettable diagnostics only.

Output root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIID8/20260506T142407Z`

Validation tier: `Integrated`.

## Build And Run

| Check | Result | Evidence |
|---|---:|---|
| `CarrierLabEditor` build | pass | `Run-CarrierLabEditorBuild.ps1`, 16.640s |
| Integrated IIID.8 replay 0 | measured / expected fail | commandlet exit code `1` because IIID.8 quantitative gate still fails |
| Active replay 1 | skipped | context-aware skip after replay 0 failed investigation gate |

## Paper Baseline Comparison

Paper Table 2 reports tectonic-process cost at 60k samples / 40 plates as `0.19s` per timestep. This instrumented run took `228.546042s` over 32 steps for the IIID-active replay 0.

| Run | Active replay 0 seconds | Seconds / step | Paper baseline | Ratio |
|---|---:|---:|---:|---:|
| Old IIID.8 integrated replay 0 | 1151.130000 | 35.972813 | 0.190000 | 189.33x |
| Pre-IIIE.4 instrumented replay 0 | 264.030493 | 8.250953 | 0.190000 | 43.43x |
| Pre-IIIE.5 D1-split replay 0 | 290.510730 | 9.078460 | 0.190000 | 47.78x |
| Pre-IIIE.6 D7-split replay 0 | 228.546042 | 7.142064 | 0.190000 | 37.59x |

Interpretation: this run is faster than Pre-IIIE.5 but still far above the tracked `10x` soft target. The cost finding remains open.

## Gate Summary

| Gate | Result | Evidence |
|---|---:|---|
| Slice 5.5 fixed-fixture regression | pass | state `3b4a85366dab80db` / `3b4a85366dab80db`, ledger `bc3077100ba291b4` / `bc3077100ba291b4` |
| IIIB independent signature regression | pass | replay A `bf8818a26ed7b1dc`, replay B `bf8818a26ed7b1dc`, expected `bf8818a26ed7b1dc` |
| Baseline primary replay stable | pass | replay A `1936a14fde702261`, replay B `1936a14fde702261`, ledger `bc3077100ba291b4` / `bc3077100ba291b4` |
| Uniform-oceanic single-hit loss reduction | fail | baseline -0.518781666863, IIID -0.594389330059, reduction -14.57%, target 80.00% |
| Collision-transfer attribution | fail | eliminated -0.075607663196, transferred 6.340432106644, attribution -8385.96%, target 50.00% |
| No lab multi-hit policy influence | pass | baseline 0 / 0, IIID 0 / 0 |

## Integrated Collision Timing

| Fixture | Replay | Attempts | Events | Collision probe seconds | Mutation seconds | No-mutation seconds | Seconds / attempt | Seconds / event | Nested calls |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---|
| iiid_active_primary_60k | 0 | 32 | 10 | 217.860991 | 204.715269 | 13.145722 | 6.808155972 | 20.471526940 | D1=32 D2=0 D3=0 D4=0 D5=0 D6=10 D7p=0 D7a=32 |
| iiid_active_primary_60k | 1 | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP |

## D7 Apply Split

| Fixture | Replay | D7 apply calls | Total seconds | Input pipeline | Uplift plan | Apply-from-plan | Topology mutation | Record apply | Projection refresh | Hash |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| iiid_active_primary_60k | 0 | 32 | 217.859891 | 214.184248 | 1.956559 | 1.719064 | 0.892821 | 0.000291 | 0.825407 | 0.000217 |
| iiid_active_primary_60k | 1 | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP |

| D7 sub-row | Seconds | Share of D7 total |
|---|---:|---:|
| Input pipeline (D1-D5 private path) | 214.184248 | 98.31% |
| Uplift plan | 1.956559 | 0.90% |
| Apply-from-plan | 1.719064 | 0.79% |
| Topology mutation inside apply-from-plan | 0.892821 | 0.41% |
| Projection refresh after uplift | 0.825407 | 0.38% |
| Record elevation/fold apply | 0.000291 | <0.01% |
| Final uplift hash | 0.000217 | <0.01% |

## D7 Apply Counts

| Fixture | Replay | Planned records | Applied records | Applied topology mutations | No-uplift attempts |
|---|---:|---:|---:|---:|---:|
| iiid_active_primary_60k | 0 | 22135 | 22135 | 10 | 22 |
| iiid_active_primary_60k | 1 | SKIP | SKIP | SKIP | SKIP |

## D1 Cross-Check

| Fixture | Replay | D1 calls | Measured D1 seconds | Component expansion | Record construction | Records |
|---|---:|---:|---:|---:|---:|---:|
| iiid_active_primary_60k | 0 | 32 | 18.926844 | 17.171521 | 1.742569 | 13787 |
| iiid_active_primary_60k | 1 | SKIP | SKIP | SKIP | SKIP | SKIP |

Pre-IIIE.5 concluded that D1 is not the dominant integrated cost. This run confirms it: D1 accounts for `18.926844s` of the `214.184248s` D7 input pipeline, about `8.84%`. The remaining repeated D2-D5 private input pipeline accounts for roughly `195.257404s`, or about `91.16%` of the input pipeline.

## Interpretation

The previous hypothesis that D7 apply/topology/uplift itself might dominate is mostly wrong. The actual topology mutation plus post-uplift projection totals about `1.718228s` across 10 applied collision events, while direct elevation/fold record application is effectively free at this resolution.

The dominant cost is the repeated input pipeline inside the public D7 apply call. D7 apply invokes the D1-D5 chain every attempted timestep; public call counters show D2-D5 as zero because the staged private `FromInputs` helpers are used, but the work is still being performed inside the D7 input pipeline. That repeated private pipeline is now the performance target.

The IIID.8 quantitative gate still fails. This report must not be used to claim Slice 5.5, Stage 1.5, or thesis remeshing has been solved. It only narrows the performance diagnosis.

## Verdict

MEASURED. The Pre-IIIE.6 question is answered: D7 apply is expensive, but not because topology mutation, uplift record application, or projection refresh dominate. The remaining hot surface is the repeated D2-D5 private input pipeline inside D7 apply. Keep the performance yellow flag open and target a D7 input-pipeline split next, especially `BuildPhaseIIID2CollisionGroupsFromTerranes`, `BuildPhaseIIID3DestinationMassFromInputs`, `BuildPhaseIIID4SlabBreakFromInputs`, and `BuildPhaseIIID5SutureFromSlabBreak`.
