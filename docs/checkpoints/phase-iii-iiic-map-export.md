# Phase III IIIC Map Export Checkpoint

Status: read-only spatial sanity export for the consolidated IIIC process layer.

## Scope

This checkpoint exports filled Mollweide-style PNG artifacts after the consolidated IIIC process layer has run with slab pull off. The export path is read-only: it must not add process mutation, resampling behavior, triangle consumption, material transfer, forbidden authority fallback patterns, or projection-derived carrier authority.

Fixtures:

- `process_layer_default`: Consolidated IIIC process-layer fixture: marks, trench elevation split, and overriding uplift enabled; slab pull remains off. (`10k / 2 plates / seed 42 / 1 rigid steps / forced_convergence motion / mixed_plate0_continental material / IIIC process on / slab pull off / centroid policy`).

## Gate Summary

| Gate | Result | Evidence |
|---|---:|---|
| map exports written | pass | output root `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260502T233818Z` |
| `process_layer_default` export read-only | pass | replay 0 `unchanged hashes`, replay 1 `unchanged hashes` |
| `process_layer_default` same-seed map hashes | pass | replay hashes byte-identical per layer |

## Tracking State Used For Maps

| Scenario | Replay | Step | Active triangles | Distance records | Matrix pairs | Polarity decisions | Subduction polarity | Propagation seeds | Propagation added | Convergence hash |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| `process_layer_default` | 0 | 1 | 446 | 446 | 1 | 1 | 1 | 84 | 84 | `51f1c267444ff160` |
| `process_layer_default` | 1 | 1 | 446 | 446 | 1 | 1 | 1 | 84 | 84 | `51f1c267444ff160` |

## IIIC State Used For Maps

| Scenario | Replay | Process layer | Slab pull | Marks | Trench records | Uplift records | Ledger records | Actual delta km | Ledger residual km | Mark hash | Elevation hash | Uplift hash | Ledger hash |
|---|---:|---|---|---:|---:|---:|---:|---:|---:|---|---|---|---|
| `process_layer_default` | 0 | on | off | 84 | 149 | 11040 | 11189 | -51.176588935012 | 1.854516540334e-12 | `d32e7289d548e202` | `a5bbbb2372fed5d1` | `74b54dd1f052ced4` | `a27dee465fa17ffb` |
| `process_layer_default` | 1 | on | off | 84 | 149 | 11040 | 11189 | -51.176588935012 | 1.854516540334e-12 | `d32e7289d548e202` | `a5bbbb2372fed5d1` | `74b54dd1f052ced4` | `a27dee465fa17ffb` |

## Exported Maps

| Scenario | Replay | Layer | Hash | Non-background pixels | Path |
|---|---:|---|---|---:|---|
| `process_layer_default` | 0 | `ElevationHeatmap` | `0b2f725974cce797` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260502T233818Z/process_layer_default/replay_0/ElevationHeatmap.png` |
| `process_layer_default` | 0 | `SubductionMask` | `fe412a294f3e908c` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260502T233818Z/process_layer_default/replay_0/SubductionMask.png` |
| `process_layer_default` | 0 | `DistanceToFrontHeatmap` | `d0d93f5f6dca6e33` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260502T233818Z/process_layer_default/replay_0/DistanceToFrontHeatmap.png` |
| `process_layer_default` | 0 | `ContactSheet` | n/a | n/a | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260502T233818Z/process_layer_default/replay_0/ContactSheet.png` |
| `process_layer_default` | 1 | `ElevationHeatmap` | `0b2f725974cce797` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260502T233818Z/process_layer_default/replay_1/ElevationHeatmap.png` |
| `process_layer_default` | 1 | `SubductionMask` | `fe412a294f3e908c` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260502T233818Z/process_layer_default/replay_1/SubductionMask.png` |
| `process_layer_default` | 1 | `DistanceToFrontHeatmap` | `d0d93f5f6dca6e33` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260502T233818Z/process_layer_default/replay_1/DistanceToFrontHeatmap.png` |
| `process_layer_default` | 1 | `ContactSheet` | n/a | n/a | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260502T233818Z/process_layer_default/replay_1/ContactSheet.png` |

## Interpretation

- `ElevationHeatmap` should show the IIIC.2 trench split and IIIC.3 overriding uplift as scalar-field color overlays on the filled continental/oceanic base map.
- `SubductionMask` should show the consolidated IIIC subducting/overriding roles produced from persistent plate-local marks, not only the earlier pre-mutation IIIB inspection overlay.
- `DistanceToFrontHeatmap` remains the front-distance spatial context for the same fixture; it is diagnostic context, not a source of authority.

## Recommendation

IIIC map export passes. These images are suitable human spatial sanity artifacts for the consolidated IIIC process layer before Phase IIID work begins.
