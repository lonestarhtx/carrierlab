# Phase III Slice IIIC.4 Checkpoint

Artifacts root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIC4/20260502T224220Z`

Status: opt-in slab pull feedback. This slice mutates only plate motion authority from IIIC subducting-triangle marks. It does not add collision, rifting, erosion, terrain displacement, projection-derived ownership, repair, recovery, backfill, or any new resampling mutation path.

Formula in code units: `omega' = clamp_v0(omega + angular(vs) * Sum(normalize(c_i x q_k)))`, where `q_k` is each subducting-front triangle barycenter, `c_i` is the current plate center, `vs=8 mm/yr`, `v0=100 mm/yr`, and `angular(v)=v*dt/R` for `dt=2 Ma`, `R=6371 km`.

## Gate Summary

| Gate | Result | Evidence |
|---|---|---|
| Slice 5.5 bypass | pass | state `3b4a85366dab80db` / `3b4a85366dab80db`, ledger `bc3077100ba291b4` / `bc3077100ba291b4` |
| IIIB closure smoke (superseded) | pass | expected independent token `bf8818a26ed7b1dc` is listed for continuity only; this standalone slice checked closure recomputation `51f1c267444ff160`, while IIIC consolidation performs the computed-vs-expected comparison |
| Slab pull opt-in on | pass | contributions 84 / 84, affected plates 1 / 1, motion `96d2f3bac07b3cda` -> `7b00b66aaa1c9e83` |
| Independent slab-pull oracle | pass | axis residual 0.000000000000e+00 / 0.000000000000e+00, angular residual 0.000000000000e+00 / 0.000000000000e+00, contribution residual 0.000000000000e+00 / 0.000000000000e+00 |
| Bounded omega | pass | max velocity 100.000000 / 100.000000 mm/yr, v0 100.000000 |
| Off/on differential | pass | disabled `96d2f3bac07b3cda` -> `96d2f3bac07b3cda`, enabled after `7b00b66aaa1c9e83` |
| Negative controls | pass | zero 0, single 0, divergence-no-subduction 0 contributions |

## Primary Forced-Convergence Replay

| Replay | Marks | Contributions | Affected plates | Max velocity mm/yr | Axis residual | Angular residual | Contribution residual | Motion before | Motion after | Slab hash | Seconds |
|---:|---:|---:|---:|---:|---:|---:|---:|---|---|---|---:|
| 0 | 84 | 84 | 1 | 100.000000 | 0.000000000000e+00 | 0.000000000000e+00 | 0.000000000000e+00 | `96d2f3bac07b3cda` | `7b00b66aaa1c9e83` | `88c9151641182bbf` | 0.072 |
| 1 | 84 | 84 | 1 | 100.000000 | 0.000000000000e+00 | 0.000000000000e+00 | 0.000000000000e+00 | `96d2f3bac07b3cda` | `7b00b66aaa1c9e83` | `88c9151641182bbf` | 0.072 |

### Affected Plate Records

| Plate | Contributions | Old angular | Raw angular | New angular | New velocity | Clamped | Old axis | New axis | Contribution sum |
|---:|---:|---:|---:|---:|---:|---|---|---|---|
| 1 | 84 | 3.767069533827e-02 | 1.837390057251e-01 | 3.139224611521e-02 | 100.000000 | yes | `(-0.183625, 0.493552, -0.850111)` | `(-0.164874, 0.494384, -0.853464)` | `(-9.308242, 28.767150, -49.689957)` |

### Representative Slab-Pull Contributions

| Mark | Plate | Other | Triangle | Signed velocity | Plate center | Front barycenter | Contribution unit |
|---:|---:|---:|---:|---:|---|---|---|
| 0 | 1 | 0 | 250 | 0.068070912065 | `(-0.049683, -0.868370, -0.493421)` | `(-0.934611, -0.110339, -0.338125)` | `(0.251488, 0.467236, -0.847611)` |
| 1 | 1 | 0 | 1706 | 0.066856551625 | `(-0.049683, -0.868370, -0.493421)` | `(-0.930935, -0.080516, -0.356199)` | `(0.281862, 0.461760, -0.841030)` |
| 2 | 1 | 0 | 1735 | 0.068565989154 | `(-0.049683, -0.868370, -0.493421)` | `(-0.939613, -0.114657, -0.322462)` | `(0.234650, 0.470055, -0.850875)` |
| 3 | 1 | 0 | 1750 | 0.070616024043 | `(-0.049683, -0.868370, -0.493421)` | `(-0.953230, -0.148068, -0.263493)` | `(0.163594, 0.480284, -0.861722)` |
| 4 | 1 | 0 | 1775 | 0.069510215478 | `(-0.049683, -0.868370, -0.493421)` | `(-0.948695, -0.123319, -0.291155)` | `(0.201112, 0.475214, -0.856578)` |
| 5 | 1 | 0 | 1805 | 0.074931574407 | `(-0.049683, -0.868370, -0.493421)` | `(-0.957629, -0.279555, -0.069248)` | `(-0.082257, 0.495911, -0.864469)` |
| 6 | 1 | 0 | 1807 | 0.072566014470 | `(-0.049683, -0.868370, -0.493421)` | `(-0.961064, -0.192106, -0.198623)` | `(0.081788, 0.488835, -0.868534)` |
| 7 | 1 | 0 | 1808 | 0.071971300039 | `(-0.049683, -0.868370, -0.493421)` | `(-0.959181, -0.177398, -0.220233)` | `(0.109099, 0.486357, -0.866923)` |
| 8 | 1 | 0 | 1819 | 0.074009601051 | `(-0.049683, -0.868370, -0.493421)` | `(-0.962452, -0.236141, -0.133875)` | `(-0.000278, 0.494043, -0.869437)` |
| 9 | 1 | 0 | 1857 | 0.075252952098 | `(-0.049683, -0.868370, -0.493421)` | `(-0.951071, -0.307857, -0.026230)` | `(-0.136664, 0.495295, -0.857908)` |
| 10 | 1 | 0 | 1860 | 0.075309485684 | `(-0.049683, -0.868370, -0.493421)` | `(-0.942052, -0.332845, 0.041865)` | `(-0.211360, 0.491984, -0.844558)` |
| 11 | 1 | 0 | 1862 | 0.075325486327 | `(-0.049683, -0.868370, -0.493421)` | `(-0.946815, -0.321745, -0.004741)` | `(-0.163742, 0.494429, -0.853656)` |
| 12 | 1 | 0 | 1904 | 0.072946821530 | `(-0.049683, -0.868370, -0.493421)` | `(-0.877002, -0.430641, 0.213110)` | `(-0.418488, 0.466672, -0.779157)` |
| 13 | 1 | 0 | 1913 | 0.066287888042 | `(-0.049683, -0.868370, -0.493421)` | `(-0.767857, -0.501281, 0.398891)` | `(-0.617837, 0.414885, -0.667944)` |
| 14 | 1 | 0 | 1956 | 0.058806709211 | `(-0.049683, -0.868370, -0.493421)` | `(-0.645042, -0.564657, 0.514862)` | `(-0.753333, 0.356948, -0.552338)` |
| 15 | 1 | 0 | 1981 | 0.063183964523 | `(-0.049683, -0.868370, -0.493421)` | `(-0.713117, -0.538167, 0.449267)` | `(-0.683217, 0.389907, -0.617403)` |

## Negative And Off-State Controls

| Fixture | Marks | Contributions | Motion before | Motion after | Closure matches | Result |
|---|---:|---:|---|---|---|---|
| slab_pull_disabled | 84 | 0 | `96d2f3bac07b3cda` | `96d2f3bac07b3cda` | yes | pass |
| zero_motion | 0 | 0 | `84ba588e474acf95` | `84ba588e474acf95` | yes | pass |
| single_plate | 0 | 0 | `b42b5f5c6fc22793` | `b42b5f5c6fc22793` | yes | pass |
| forced_divergence_no_subduction | 0 | 0 | `aa96c1b974567fbe` | `aa96c1b974567fbe` | yes | pass |

## Scope Notes

- Slab pull is opt-in and defaults off on the actor. The commandlet enables it only for the primary on-state fixture.
- The off-state fixture has IIIC marks present but slab pull disabled, so marks alone cannot mutate motion authority.
- The independent oracle recomputes `normalize(c_i x q_k)`, contribution sums, speed clamping, and axis decomposition from audit geometry and constants.
- This checkpoint may claim only IIIC.4 slab-pull feedback behavior. It does not claim Stage 1.5 carrier success, Slice 5.5 asymmetry resolution, collision, rifting, erosion, or terrain morphology.

## Recommendation

IIIC.4 passes. Pause for user review before IIIC consolidation or IIID planning.
