# Phase III Observability Map Export Checkpoint

Status: read-only observability patch before IIIC entry reconciliation.

## Scope

This checkpoint exports the Phase III actor-only spatial sanity layers to PNG artifacts. It does not add process mutation, resampling behavior, triangle consumption, material transfer, forbidden authority fallback patterns, or projection-derived carrier authority.

Fixtures:

- `default_40_plate_baseline`: Default 40-plate pre-IIIC baseline. Elevation is expected to be flat, and IIIB-derived masks may be sparse before IIIC persistent marks exist. (`60k / 40 plates / seed 42 / 40 rigid steps / default motion / default material / centroid policy`).
- `forced_convergence_mixed`: Two-plate forced-convergence mixed-material fixture that exercises IIIB polarity roles and distance-to-front masks for human spatial sanity checks. (`60k / 2 plates / seed 42 / 8 rigid steps / forced_convergence motion / mixed_plate0_continental material / centroid policy`).

## Gate Summary

| Gate | Result | Evidence |
|---|---:|---|
| map exports written | pass | output root `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/Observability/Maps/20260502T204636Z` |
| `default_40_plate_baseline` export read-only | pass | replay 0 `unchanged hashes`, replay 1 `unchanged hashes` |
| `default_40_plate_baseline` same-seed map hashes | pass | replay hashes byte-identical per layer |
| `forced_convergence_mixed` export read-only | pass | replay 0 `unchanged hashes`, replay 1 `unchanged hashes` |
| `forced_convergence_mixed` same-seed map hashes | pass | replay hashes byte-identical per layer |

## Tracking State Used For Maps

| Scenario | Replay | Step | Active triangles | Distance records | Matrix pairs | Polarity decisions | Subduction polarity | Propagation seeds | Propagation added | Convergence hash |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| `default_40_plate_baseline` | 0 | 40 | 0 | 0 | 75 | 75 | 17 | 0 | 0 | `d145f51eab69d5d8` |
| `default_40_plate_baseline` | 1 | 40 | 0 | 0 | 75 | 75 | 17 | 0 | 0 | `d145f51eab69d5d8` |
| `forced_convergence_mixed` | 0 | 8 | 1718 | 1718 | 1 | 1 | 1 | 1089 | 108 | `da229234152f077b` |
| `forced_convergence_mixed` | 1 | 8 | 1718 | 1718 | 1 | 1 | 1 | 1089 | 108 | `da229234152f077b` |

## Exported Maps

| Scenario | Replay | Layer | Hash | Non-background pixels | Path |
|---|---:|---|---|---:|---|
| `default_40_plate_baseline` | 0 | `ElevationHeatmap` | `f7cd0d6761fba4c7` | 539913 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/Observability/Maps/20260502T204636Z/default_40_plate_baseline/replay_0/ElevationHeatmap.png` |
| `default_40_plate_baseline` | 0 | `SubductionMask` | `687449314ef0ccf5` | 539913 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/Observability/Maps/20260502T204636Z/default_40_plate_baseline/replay_0/SubductionMask.png` |
| `default_40_plate_baseline` | 0 | `DistanceToFrontHeatmap` | `687449314ef0ccf5` | 539913 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/Observability/Maps/20260502T204636Z/default_40_plate_baseline/replay_0/DistanceToFrontHeatmap.png` |
| `default_40_plate_baseline` | 0 | `ContactSheet` | n/a | n/a | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/Observability/Maps/20260502T204636Z/default_40_plate_baseline/replay_0/ContactSheet.png` |
| `default_40_plate_baseline` | 1 | `ElevationHeatmap` | `f7cd0d6761fba4c7` | 539913 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/Observability/Maps/20260502T204636Z/default_40_plate_baseline/replay_1/ElevationHeatmap.png` |
| `default_40_plate_baseline` | 1 | `SubductionMask` | `687449314ef0ccf5` | 539913 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/Observability/Maps/20260502T204636Z/default_40_plate_baseline/replay_1/SubductionMask.png` |
| `default_40_plate_baseline` | 1 | `DistanceToFrontHeatmap` | `687449314ef0ccf5` | 539913 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/Observability/Maps/20260502T204636Z/default_40_plate_baseline/replay_1/DistanceToFrontHeatmap.png` |
| `default_40_plate_baseline` | 1 | `ContactSheet` | n/a | n/a | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/Observability/Maps/20260502T204636Z/default_40_plate_baseline/replay_1/ContactSheet.png` |
| `forced_convergence_mixed` | 0 | `ElevationHeatmap` | `f7cd0d6761fba4c7` | 539913 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/Observability/Maps/20260502T204636Z/forced_convergence_mixed/replay_0/ElevationHeatmap.png` |
| `forced_convergence_mixed` | 0 | `SubductionMask` | `b98fda446b9db95d` | 539913 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/Observability/Maps/20260502T204636Z/forced_convergence_mixed/replay_0/SubductionMask.png` |
| `forced_convergence_mixed` | 0 | `DistanceToFrontHeatmap` | `bce0b861492d1000` | 539913 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/Observability/Maps/20260502T204636Z/forced_convergence_mixed/replay_0/DistanceToFrontHeatmap.png` |
| `forced_convergence_mixed` | 0 | `ContactSheet` | n/a | n/a | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/Observability/Maps/20260502T204636Z/forced_convergence_mixed/replay_0/ContactSheet.png` |
| `forced_convergence_mixed` | 1 | `ElevationHeatmap` | `f7cd0d6761fba4c7` | 539913 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/Observability/Maps/20260502T204636Z/forced_convergence_mixed/replay_1/ElevationHeatmap.png` |
| `forced_convergence_mixed` | 1 | `SubductionMask` | `b98fda446b9db95d` | 539913 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/Observability/Maps/20260502T204636Z/forced_convergence_mixed/replay_1/SubductionMask.png` |
| `forced_convergence_mixed` | 1 | `DistanceToFrontHeatmap` | `bce0b861492d1000` | 539913 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/Observability/Maps/20260502T204636Z/forced_convergence_mixed/replay_1/DistanceToFrontHeatmap.png` |
| `forced_convergence_mixed` | 1 | `ContactSheet` | n/a | n/a | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/Observability/Maps/20260502T204636Z/forced_convergence_mixed/replay_1/ContactSheet.png` |

## Interpretation

- `ElevationHeatmap` is expected to show the pre-IIIC zero-elevation baseline; it should become visually informative once IIIC.2/IIIC.3 mutate elevation.
- `SubductionMask` visualizes IIIB polarity-derived roles from current read-only tracking state; the forced-convergence fixture is the human-inspection map, not persistent IIIC subducting-triangle authority.
- `DistanceToFrontHeatmap` visualizes active boundary distance-to-front records; the default baseline may be sparse, while the forced fixture intentionally exercises propagated front state.

## Recommendation

Go for the docs-only IIIC entry reconciliation checkpoint. These exports are read-only and stable enough to serve as pre-mutation spatial sanity artifacts.
