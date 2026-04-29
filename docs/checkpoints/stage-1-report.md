# Stage 1 Checkpoint: Rigid Motion Preservation

Stage 1 uses per-step rigid vertex rotation and projects fixed global samples by ray-from-planet-center queries against rebuilt per-plate `FDynamicMeshAABBTree3` BVHs. There is no resampling, no mutation, no ownership persistence, and no Stage 0 incident-triangle shortcut.

Artifacts root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/Stage1/target_20260428`

BoundaryMask rendering note: white means the ray hit at least one plate-local triangle on a barycentric edge/vertex within epsilon; black means no boundary-degenerate hit. It is a hit-degeneracy diagnostic, not an area-fill measure, so it can look visually heavier than the count-based metric.

## Main Run Metrics

| Resolution | Step | Miss % | Multi-hit % | CAF | Third-plate | Drift expected km | Drift observed km | Drift err mean km | Drift err p95 km | Kernel s | Memory GB | Hash |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| 60000 | 0 | 0.000000 | 8.183333 | 0.301050 | 82 | 0.000 | 0.000 | 0.000001597 | 0.000000000 | 0.286174 | 1.435 | `d9aec415886b4b3f` |
| 60000 | 100 | 32.916667 | 28.628333 | 0.192550 | 2529 | 2204.327 | 2204.327 | 0.000005078 | 0.000000000 | 0.255889 | 1.440 | `669e8eb8ce4ef09b` |
| 60000 | 200 | 36.308333 | 25.180000 | 0.188917 | 5213 | 4406.605 | 4406.605 | 0.000004255 | 0.000000000 | 0.253182 | 1.440 | `ba4331bbfe980e0c` |
| 60000 | 400 | 41.588333 | 27.721667 | 0.187750 | 6171 | 8793.370 | 8793.370 | 0.000003712 | 0.000000000 | 0.250720 | 1.441 | `4cd7fc964aa3910c` |
| 100000 | 0 | 0.000000 | 5.845000 | 0.301040 | 84 | 0.000 | 0.000 | 0.000001605 | 0.000000000 | 0.465717 | 1.479 | `5c2c0ba71ff9eaf6` |
| 100000 | 100 | 32.967000 | 28.679000 | 0.192460 | 4177 | 2204.849 | 2204.849 | 0.000005544 | 0.000094935 | 0.430149 | 1.471 | `63b4696f197cde8a` |
| 100000 | 200 | 36.300000 | 25.140000 | 0.188840 | 8661 | 4407.680 | 4407.680 | 0.000004180 | 0.000000000 | 0.429047 | 1.471 | `892894071d1c1a4d` |
| 100000 | 400 | 41.591000 | 27.676000 | 0.187420 | 10288 | 8795.811 | 8795.811 | 0.000003395 | 0.000000000 | 0.435263 | 1.477 | `78c498ed371c5262` |
| 250000 | 0 | 0.000000 | 3.910800 | 0.301056 | 76 | 0.000 | 0.000 | 0.000001686 | 0.000000000 | 1.257797 | 1.619 | `52ec32208a45ad47` |
| 250000 | 100 | 32.970000 | 28.696800 | 0.192552 | 10375 | 2205.437 | 2205.437 | 0.000005627 | 0.000094935 | 1.173197 | 1.629 | `07c450af5ed4344e` |
| 250000 | 200 | 36.307600 | 25.132000 | 0.188644 | 21676 | 4408.890 | 4408.890 | 0.000004196 | 0.000000000 | 1.199468 | 1.629 | `91013d1c71a5f38e` |
| 250000 | 400 | 41.570800 | 27.682800 | 0.187612 | 25695 | 8798.563 | 8798.563 | 0.000003638 | 0.000000000 | 1.151185 | 1.630 | `4c65c3f19b5173ae` |
| 500000 | 0 | 0.000000 | 2.756000 | 0.301078 | 88 | 0.000 | 0.000 | 0.000001668 | 0.000000000 | 2.633967 | 1.887 | `1f76b4d4ddebc4cd` |
| 500000 | 100 | 32.955600 | 28.697800 | 0.192662 | 20895 | 2205.799 | 2205.799 | 0.000005502 | 0.000094935 | 2.425681 | 1.894 | `a8a9f9532d7873fb` |
| 500000 | 200 | 36.308800 | 25.149200 | 0.188780 | 43348 | 4409.637 | 4409.637 | 0.000004174 | 0.000000000 | 2.444525 | 1.896 | `cbf3281591938b54` |
| 500000 | 400 | 41.575400 | 27.695600 | 0.187638 | 51303 | 8800.260 | 8800.260 | 0.000003472 | 0.000000000 | 2.484009 | 1.896 | `0f3e12f927d2cc53` |

## Aurous Baseline Comparison

| Metric | Lab | Aurous failed prototype | Parameters match | Note |
|---|---:|---:|---|---|
| Miss rate step 100 | 0.329700000000 | 0.330000000000 | true | 250k/40/seed-42 failure memo baseline |
| Multi-hit rate step 100 | 0.286968000000 | 0.240000000000 | true | 250k/40/seed-42 failure memo baseline |
| CAF step 400 | 0.187612000000 | 0.000600000000 | true | Failure memo CAF-collapse baseline |
| Drift observed vs expected step 400 | 8798.563 / 8798.563 km | 524 / 8950 km | partial | Memo records observed-vs-expected, not mean angular error |

## Classified Counts

| Resolution | Step | Raw hit | Raw miss | Raw multi | Divergent gap | Numeric miss | Boundary-degenerate | Convergent overlap | Third-plate intrusion | NaN/Inf |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 60000 | 0 | 60000 | 0 | 4910 | 0 | 0 | 4828 | 0 | 82 | 0 |
| 60000 | 100 | 40250 | 19750 | 17177 | 9416 | 10334 | 0 | 14648 | 2529 | 0 |
| 60000 | 200 | 38215 | 21785 | 15108 | 11813 | 9972 | 0 | 9895 | 5213 | 0 |
| 60000 | 400 | 35047 | 24953 | 16633 | 12867 | 12086 | 0 | 10462 | 6171 | 0 |
| 100000 | 0 | 100000 | 0 | 5845 | 0 | 0 | 5761 | 0 | 84 | 0 |
| 100000 | 100 | 67033 | 32967 | 28679 | 15724 | 17243 | 0 | 24502 | 4177 | 0 |
| 100000 | 200 | 63700 | 36300 | 25140 | 19677 | 16623 | 0 | 16479 | 8661 | 0 |
| 100000 | 400 | 58409 | 41591 | 27676 | 21444 | 20147 | 0 | 17388 | 10288 | 0 |
| 250000 | 0 | 250000 | 0 | 9777 | 0 | 0 | 9701 | 0 | 76 | 0 |
| 250000 | 100 | 167575 | 82425 | 71742 | 39265 | 43160 | 0 | 61367 | 10375 | 0 |
| 250000 | 200 | 159231 | 90769 | 62830 | 49233 | 41536 | 0 | 41154 | 21676 | 0 |
| 250000 | 400 | 146073 | 103927 | 69207 | 53572 | 50355 | 0 | 43512 | 25695 | 0 |
| 500000 | 0 | 500000 | 0 | 13780 | 0 | 0 | 13692 | 0 | 88 | 0 |
| 500000 | 100 | 335222 | 164778 | 143489 | 78390 | 86388 | 0 | 122594 | 20895 | 0 |
| 500000 | 200 | 318456 | 181544 | 125746 | 98483 | 83061 | 0 | 82398 | 43348 | 0 |
| 500000 | 400 | 292123 | 207877 | 138478 | 107190 | 100687 | 0 | 87175 | 51303 | 0 |

## Stage 1 Read

The motion oracle is strong: observed continental material displacement matches the analytic rotation at every resolution, and mean drift error stays at micrometer scale in Earth-kilometer units. That rules out the anchored-continent failure shape for this clean-room Stage 1 run.

Coverage is not clean under long no-resampling motion. At 250k/40/seed-42 step 100, raw miss rate is 32.9700% versus Aurous's 33%, and raw multi-hit rate is 28.6968% versus Aurous's 24%. This reproduces the raw high-miss/high-multi part of the Aurous signature, but not the full cocktail: CAF remains 0.187612 at step 400 instead of collapsing to 0.0006, drift is exact rather than anchored, and the maps show coherent moving plate footprints rather than speckled material noise.

Interpretation: Stage 1 proves rigid plate-local material transport under per-step vertex rotation, but it also shows that no-resampling coverage degrades to Aurous-scale raw gaps/overlaps by step 100. The targeted next question is Stage 1.5: whether the thesis resampling pass closes these gaps and resolves overlaps without CAF collapse.


## Determinism

Same-seed replay hashes matched across all main Stage 1 checkpoints.

| Resolution | Step | Hash A | Hash B | Match |
|---:|---:|---|---|---|
| 60000 | 0 | `d9aec415886b4b3f` | `d9aec415886b4b3f` | yes |
| 60000 | 100 | `669e8eb8ce4ef09b` | `669e8eb8ce4ef09b` | yes |
| 60000 | 200 | `ba4331bbfe980e0c` | `ba4331bbfe980e0c` | yes |
| 60000 | 400 | `4cd7fc964aa3910c` | `4cd7fc964aa3910c` | yes |
| 100000 | 0 | `5c2c0ba71ff9eaf6` | `5c2c0ba71ff9eaf6` | yes |
| 100000 | 100 | `63b4696f197cde8a` | `63b4696f197cde8a` | yes |
| 100000 | 200 | `892894071d1c1a4d` | `892894071d1c1a4d` | yes |
| 100000 | 400 | `78c498ed371c5262` | `78c498ed371c5262` | yes |
| 250000 | 0 | `52ec32208a45ad47` | `52ec32208a45ad47` | yes |
| 250000 | 100 | `07c450af5ed4344e` | `07c450af5ed4344e` | yes |
| 250000 | 200 | `91013d1c71a5f38e` | `91013d1c71a5f38e` | yes |
| 250000 | 400 | `4c65c3f19b5173ae` | `4c65c3f19b5173ae` | yes |
| 500000 | 0 | `1f76b4d4ddebc4cd` | `1f76b4d4ddebc4cd` | yes |
| 500000 | 100 | `a8a9f9532d7873fb` | `a8a9f9532d7873fb` | yes |
| 500000 | 200 | `cbf3281591938b54` | `cbf3281591938b54` | yes |
| 500000 | 400 | `0f3e12f927d2cc53` | `0f3e12f927d2cc53` | yes |

## Negative Controls

| Control | Step | Miss | Multi-hit | CAF | Drift err mean km | Hash | Gate |
|---|---:|---:|---:|---:|---:|---|---|
| zero-motion (40 plates, 10000 samples) | 0 | 0 | 1859 | 0.300300 | 0.000001618 | `2848034669093276` | inspect |
| zero-motion (40 plates, 10000 samples) | 100 | 0 | 1859 | 0.300300 | 0.000000937 | `2848034669093276` | pass |
| single-plate (1 plates, 10000 samples) | 0 | 0 | 0 | 0.000000 | 0.000000000 | `f285e3f104feaeb1` | pass |
| single-plate (1 plates, 10000 samples) | 100 | 0 | 0 | 0.000000 | 0.000000000 | `f285e3f104feaeb1` | pass |
| forced-convergence (2 plates, 10000 samples) | 0 | 0 | 276 | 0.500300 | 0.000002357 | `892b49fa6374697d` | inspect |
| forced-convergence (2 plates, 10000 samples) | 100 | 1994 | 2011 | 0.400600 | 0.000004461 | `4ce0c1449925f1a0` | inspect |
| forced-divergence (2 plates, 10000 samples) | 0 | 0 | 276 | 0.500300 | 0.000002357 | `892b49fa6374697d` | inspect |
| forced-divergence (2 plates, 10000 samples) | 100 | 2011 | 1994 | 0.399900 | 0.000004392 | `7235ee6133b96c25` | inspect |

Control gate summary: pass.

## Visual Exports

Each `main/<resolution>/step_###` folder contains `PlateId.png`, `MissMask.png`, `OverlapMask.png`, `BoundaryMask.png`, `ContinentalFraction.png`, `ThirdPlateIntrusion.png`, `DriftErrorMap.png`, and `ContactSheet.png`.

## Recommendation

Conditional go for user review: Stage 1 passes rigid material transport, drift, determinism, runtime, and negative-control checks, but it reproduces Aurous-scale raw coverage gaps/overlaps when motion runs without resampling. My recommendation is to approve Stage 1.5 as the investigation of this exact coverage defect, not to advance to Stage 2 or declare the carrier viable from Stage 1 alone.
