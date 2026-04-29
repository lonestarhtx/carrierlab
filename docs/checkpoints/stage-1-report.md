# Stage 1 Checkpoint: Rigid Motion Preservation

Stage 1 uses per-step rigid vertex rotation and projects fixed global samples by ray-from-planet-center queries against rebuilt per-plate `FDynamicMeshAABBTree3` BVHs. There is no resampling, no mutation, no ownership persistence, and no Stage 0 incident-triangle shortcut.

Artifacts root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/Stage1/target_20260428`

BoundaryMask rendering note: white means the ray hit at least one plate-local triangle on a barycentric edge/vertex within epsilon; black means no boundary-degenerate hit. It is a hit-degeneracy diagnostic, not an area-fill measure, so it can look visually heavier than the count-based metric.

## Main Run Metrics

| Resolution | Step | Miss % | Multi-hit % | Auth CAF | Proj CAF | Proj loss vs auth % | Third-plate | Drift expected km | Drift observed km | Drift err mean km | Drift err p95 km | Kernel s | Memory GB | Hash |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| 60000 | 0 | 0.000000 | 8.183333 | 0.301050 | 0.301050 | 0.000 | 82 | 0.000 | 0.000 | 0.000001597 | 0.000000000 | 0.274232 | 1.441 | `7c3c557782675d12` |
| 60000 | 100 | 32.916667 | 28.628333 | 0.301050 | 0.192550 | 36.041 | 2529 | 2204.327 | 2204.327 | 0.000005078 | 0.000000000 | 0.254644 | 1.446 | `73ea194c8caee4a5` |
| 60000 | 200 | 36.308333 | 25.180000 | 0.301050 | 0.188917 | 37.247 | 5213 | 4406.605 | 4406.605 | 0.000004255 | 0.000000000 | 0.253766 | 1.446 | `ec6537edec90385a` |
| 60000 | 400 | 41.588333 | 27.721667 | 0.301050 | 0.187750 | 37.635 | 6171 | 8793.370 | 8793.370 | 0.000003712 | 0.000000000 | 0.252931 | 1.446 | `a0114d2a75f6b116` |
| 100000 | 0 | 0.000000 | 5.845000 | 0.301040 | 0.301040 | 0.000 | 84 | 0.000 | 0.000 | 0.000001605 | 0.000000000 | 0.466647 | 1.485 | `f831044ed1dfd0bd` |
| 100000 | 100 | 32.967000 | 28.679000 | 0.301040 | 0.192460 | 36.068 | 4177 | 2204.849 | 2204.849 | 0.000005544 | 0.000094935 | 0.429744 | 1.477 | `b312021adbada330` |
| 100000 | 200 | 36.300000 | 25.140000 | 0.301040 | 0.188840 | 37.271 | 8661 | 4407.680 | 4407.680 | 0.000004180 | 0.000000000 | 0.430768 | 1.477 | `5ccfbe76dd12ed5e` |
| 100000 | 400 | 41.591000 | 27.676000 | 0.301040 | 0.187420 | 37.742 | 10288 | 8795.811 | 8795.811 | 0.000003395 | 0.000000000 | 0.431968 | 1.483 | `bf435cd43872fd90` |
| 250000 | 0 | 0.000000 | 3.910800 | 0.301056 | 0.301056 | 0.000 | 76 | 0.000 | 0.000 | 0.000001686 | 0.000000000 | 1.262852 | 1.625 | `97783e399362b905` |
| 250000 | 100 | 32.970000 | 28.696800 | 0.301056 | 0.192552 | 36.041 | 10375 | 2205.437 | 2205.437 | 0.000005627 | 0.000094935 | 1.156294 | 1.635 | `2144454d95a3e12d` |
| 250000 | 200 | 36.307600 | 25.132000 | 0.301056 | 0.188644 | 37.339 | 21676 | 4408.890 | 4408.890 | 0.000004196 | 0.000000000 | 1.167211 | 1.635 | `c5b818ec241df791` |
| 250000 | 400 | 41.570800 | 27.682800 | 0.301056 | 0.187612 | 37.682 | 25695 | 8798.563 | 8798.563 | 0.000003638 | 0.000000000 | 1.147552 | 1.636 | `07f23d2103affa1f` |
| 500000 | 0 | 0.000000 | 2.756000 | 0.301078 | 0.301078 | 0.000 | 88 | 0.000 | 0.000 | 0.000001668 | 0.000000000 | 2.628002 | 1.893 | `c7e22cc33bcada58` |
| 500000 | 100 | 32.955600 | 28.697800 | 0.301078 | 0.192662 | 36.009 | 20895 | 2205.799 | 2205.799 | 0.000005502 | 0.000094935 | 2.432647 | 1.901 | `7970bd4feb06c64d` |
| 500000 | 200 | 36.308800 | 25.149200 | 0.301078 | 0.188780 | 37.299 | 43348 | 4409.637 | 4409.637 | 0.000004174 | 0.000000000 | 2.800616 | 1.902 | `1d96e37bcfe7ac51` |
| 500000 | 400 | 41.575400 | 27.695600 | 0.301078 | 0.187638 | 37.678 | 51303 | 8800.260 | 8800.260 | 0.000003472 | 0.000000000 | 2.825773 | 1.902 | `d767c610c3e33f09` |

## Aurous Baseline Comparison

| Metric | Lab | Aurous failed prototype | Parameters match | Note |
|---|---:|---:|---|---|
| Miss rate step 100 | 0.329700000000 | 0.330000000000 | true | 250k/40/seed-42 failure memo baseline |
| Multi-hit rate step 100 | 0.286968000000 | 0.240000000000 | true | 250k/40/seed-42 failure memo baseline |
| Projected CAF step 400 | 0.187612000001 | 0.000600000000 | true | Failure memo CAF-collapse baseline; lab authoritative CAF is 0.301056000000 |
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

The motion oracle is strong: observed continental material displacement matches the analytic rotation at every resolution, and mean drift error stays at micrometer scale in Earth-kilometer units. This means the clean-room Stage 1 run does not reproduce Aurous's anchored-continent drift failure in the rigid-motion-only subset.

Coverage is not clean under long no-resampling motion. At 250k/40/seed-42 step 100, raw miss rate is 32.9700% versus Aurous's 33%, and raw multi-hit rate is 28.6968% versus Aurous's 24%. This is similar but not exact: miss is close, while multi-hit differs by 10,555 samples at step 100. By step 400, projected CAF is 0.187612 while authoritative CAF remains about 0.301, a projected loss of about 37.5% relative to authority. That is not material preservation in projection; it is only evidence that the clean-room rigid subset does not collapse as catastrophically as Aurous's 0.0006 CAF run.

Interpretation: Aurous almost certainly had implementation or architecture issues, because Stage 1 keeps rigid plate-local drift analytic and avoids the catastrophic CAF collapse. Stage 1 does not identify which Aurous bug caused the collapse. A third explanation remains live: Aurous's resampling path may have been broken in a way that both failed to close geometric gaps and destroyed material. The targeted next question is Stage 1.5: whether thesis-cadence resampling closes the Stage 1 gaps/overlaps without increasing projected-authoritative CAF error.

## Resolver Policy Caveat

Stage 1 resolves no-subduction multi-hit samples with `ChooseNearestCandidatePlate`, a nearest-current-plate-centroid lab policy with lowest-plate-id tie-break. This is not a thesis-faithful substitute for subduction/collision exclusion. Stage 1.5 must keep raw multi-hit counts visible, label centroid-resolved samples as policy-resolved, and report per-plate projected area deltas so centroid bias is measurable.


## Determinism

Same-seed replay hashes matched across all main Stage 1 checkpoints.

| Resolution | Step | Hash A | Hash B | Match |
|---:|---:|---|---|---|
| 60000 | 0 | `7c3c557782675d12` | `7c3c557782675d12` | yes |
| 60000 | 100 | `73ea194c8caee4a5` | `73ea194c8caee4a5` | yes |
| 60000 | 200 | `ec6537edec90385a` | `ec6537edec90385a` | yes |
| 60000 | 400 | `a0114d2a75f6b116` | `a0114d2a75f6b116` | yes |
| 100000 | 0 | `f831044ed1dfd0bd` | `f831044ed1dfd0bd` | yes |
| 100000 | 100 | `b312021adbada330` | `b312021adbada330` | yes |
| 100000 | 200 | `5ccfbe76dd12ed5e` | `5ccfbe76dd12ed5e` | yes |
| 100000 | 400 | `bf435cd43872fd90` | `bf435cd43872fd90` | yes |
| 250000 | 0 | `97783e399362b905` | `97783e399362b905` | yes |
| 250000 | 100 | `2144454d95a3e12d` | `2144454d95a3e12d` | yes |
| 250000 | 200 | `c5b818ec241df791` | `c5b818ec241df791` | yes |
| 250000 | 400 | `07f23d2103affa1f` | `07f23d2103affa1f` | yes |
| 500000 | 0 | `c7e22cc33bcada58` | `c7e22cc33bcada58` | yes |
| 500000 | 100 | `7970bd4feb06c64d` | `7970bd4feb06c64d` | yes |
| 500000 | 200 | `1d96e37bcfe7ac51` | `1d96e37bcfe7ac51` | yes |
| 500000 | 400 | `d767c610c3e33f09` | `d767c610c3e33f09` | yes |

## Negative Controls

Directional gates are now numeric: forced-convergence final step requires `multi >= 2 * miss`; forced-divergence final step requires `miss >= 2 * multi`. These are deliberately stricter than appearance checks, because a near-symmetric miss/multi result does not prove the control is directional.

| Control | Step | Miss | Multi-hit | Auth CAF | Proj CAF | Drift err mean km | Hash | Gate | Expectation |
|---|---:|---:|---:|---:|---:|---:|---|---|---|
| zero-motion (40 plates, 10000 samples) | 0 | 0 | 1859 | 0.300300 | 0.300300 | 0.000001618 | `907fc82b4e126cc4` | baseline | baseline capture |
| zero-motion (40 plates, 10000 samples) | 100 | 0 | 1859 | 0.300300 | 0.300300 | 0.000000937 | `907fc82b4e126cc4` | pass | same output hash/counts as step 0 |
| single-plate (1 plates, 10000 samples) | 0 | 0 | 0 | 0.000000 | 0.000000 | 0.000000000 | `bc77371d2ede1983` | baseline | baseline capture |
| single-plate (1 plates, 10000 samples) | 100 | 0 | 0 | 0.000000 | 0.000000 | 0.000000000 | `bc77371d2ede1983` | pass | no misses, no overlaps |
| forced-convergence (2 plates, 10000 samples) | 0 | 0 | 276 | 0.500300 | 0.500300 | 0.000002357 | `cd61279e4d29674e` | baseline | baseline capture |
| forced-convergence (2 plates, 10000 samples) | 100 | 1994 | 2011 | 0.500300 | 0.400600 | 0.000004461 | `1113b4af24ffa436` | fail | directional overlap dominance: multi >= 2 * miss |
| forced-divergence (2 plates, 10000 samples) | 0 | 0 | 276 | 0.500300 | 0.500300 | 0.000002357 | `cd61279e4d29674e` | baseline | baseline capture |
| forced-divergence (2 plates, 10000 samples) | 100 | 2011 | 1994 | 0.500300 | 0.399900 | 0.000004392 | `1c6c5c85576b1386` | fail | directional gap dominance: miss >= 2 * multi |

Control gate summary: fail.

## Visual Exports

Each `main/<resolution>/step_###` folder contains `PlateId.png`, `MissMask.png`, `OverlapMask.png`, `BoundaryMask.png`, `ContinentalFraction.png`, `ThirdPlateIntrusion.png`, `DriftErrorMap.png`, and `ContactSheet.png`.

## Recommendation

No-go for Stage 1.5 implementation until the failed control gate is either fixed or explicitly re-scoped. Stage 1 remains useful evidence for rigid drift, but the forced directional controls are not yet discriminating convergence from divergence.
