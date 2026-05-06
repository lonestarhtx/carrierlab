# Phase III Pre-IIIE.5 Report - D1 Candidate/Terrane Detection Cost Split

Status: MEASURED / D1 NOT THE DOMINANT INTEGRATED COST. This checkpoint instruments the remaining `D1=32` cost from Pre-IIIE.4 by splitting `DetectPhaseIIID1ConnectedTerranes` into diagnostics-only timing/count rows. It uses the real IIID.8 60k sample / 40 plate / 32 step integrated path, with active replay 1 skipped because replay 0 still fails the IIID.8 investigation gate.

This slice adds no new collision behavior, carrier authority, remesh behavior, ownership persistence, repair, recovery, backfill, projection-derived state, or cache-as-authority. The added fields are resettable diagnostics only.

Output root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIID8/20260506T140541Z`

Validation tier: `Integrated`.

## Build And Run

| Check | Result | Evidence |
|---|---:|---|
| `CarrierLabEditor` build | pass | `Run-CarrierLabEditorBuild.ps1`, 22.999s |
| Integrated IIID.8 replay 0 | measured / expected fail | commandlet exit code `1` because IIID.8 quantitative gate still fails |
| Active replay 1 | skipped | context-aware skip after replay 0 failed investigation gate |

## Paper Baseline Comparison

Paper Table 2 reports tectonic-process cost at 60k samples / 40 plates as `0.19s` per timestep. This instrumented run took `290.510730s` over 32 steps for the IIID-active replay 0.

| Run | Active replay 0 seconds | Seconds / step | Paper baseline | Ratio |
|---|---:|---:|---:|---:|
| Old IIID.8 integrated replay 0 | 1151.130000 | 35.972813 | 0.190000 | 189.33x |
| Pre-IIIE.4 instrumented replay 0 | 264.030493 | 8.250953 | 0.190000 | 43.43x |
| Pre-IIIE.5 D1-split replay 0 | 290.510730 | 9.078460 | 0.190000 | 47.78x |

Interpretation: this run is slightly slower than Pre-IIIE.4, likely due to added timing probes and ordinary run variance. It confirms the cost remains far above the tracked `10x` soft target.

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
| iiid_active_primary_60k | 0 | 32 | 10 | 280.589726 | 264.205738 | 16.383988 | 8.768428953 | 26.420573810 | D1=32 D2=0 D3=0 D4=0 D5=0 D6=10 D7p=0 D7a=32 |
| iiid_active_primary_60k | 1 | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP |

The Pre-IIIE.3 plan-reuse result still holds: D2 through D5 nested public calls remain collapsed to zero inside active collision application. The remaining public call surface is D1 once per attempted timestep, D6 for the 10 topology mutations, and D7 apply once per attempted timestep.

## D1 Detection Split

| Fixture | Replay | D1 calls | Measured D1 seconds | Decision index | Hit sort | Hit classification | Component expansion | Inner-sea scan | Record construction | Audit hash |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| iiid_active_primary_60k | 0 | 32 | 23.040843 | 0.000184 | 0.003224 | 0.002911 | 21.332739 | 0.006204 | 1.695567 | 0.000015 |
| iiid_active_primary_60k | 1 | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP |

| D1 sub-row | Seconds | Share of measured D1 |
|---|---:|---:|
| Component expansion | 21.332739 | 92.59% |
| Record construction | 1.695567 | 7.36% |
| Inner-sea scan | 0.006204 | 0.03% |
| Hit sort | 0.003224 | 0.01% |
| Hit classification | 0.002911 | 0.01% |
| Decision index | 0.000184 | <0.01% |
| Audit hash | 0.000015 | <0.01% |

## D1 Detection Counts

| Fixture | Replay | Sorted hits | Collision candidates | Component builds | Component cache hits | Expanded continental triangles | Scanned oceanic triangles | Inner-sea triangles | Records |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| iiid_active_primary_60k | 0 | 90849 | 13788 | 380 | 13407 | 1067786 | 82 | 0 | 13787 |
| iiid_active_primary_60k | 1 | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP |

## Interpretation

D1 is not the dominant integrated cost. It accounts for `23.040843s` of the `280.589726s` collision probe, about `8.21%`. Within D1, component expansion is the clear hotspot: `21.332739s`, or `92.59%` of measured D1. The algorithm built 380 components and reused them 13,407 times, expanding 1,067,786 continental triangles across the replay.

This means the next optimization should not be framed as "D1 explains the 43x-48x paper-ratio gap." D1 component expansion is worth optimizing later, but the larger remaining cost sits outside D1, most likely in the D7 apply path and/or topology/uplift work executed during mutation attempts.

The IIID.8 quantitative gate still fails. This report must not be used to claim Slice 5.5, Stage 1.5, or thesis remeshing has been solved. It only narrows the performance diagnosis.

## Verdict

MEASURED. The Pre-IIIE.5 question is answered: D1 component expansion dominates D1, but D1 does not dominate integrated collision cost. Keep the performance yellow flag open and target a D7 apply/topology/uplift split next.
