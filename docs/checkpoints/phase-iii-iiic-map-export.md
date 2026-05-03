# Phase III IIIC Map Export Checkpoint

Status: read-only spatial sanity export for the consolidated IIIC process layer.

## Scope

This checkpoint exports filled Mollweide-style PNG artifacts after the consolidated IIIC process layer has run with slab pull off. The export path is read-only: it must not add process mutation, resampling behavior, triangle consumption, material transfer, forbidden authority fallback patterns, or projection-derived carrier authority.

Fixtures:

- `zero_motion_control`: Zero-motion control with IIIC process layer enabled. It should remain spatially blank for subduction, trench, and uplift signals. (`10k / 2 plates / seed 42 / 1 rigid steps / zero_motion motion / mixed_plate0_continental material / IIIC process on / slab pull off / expected no_process_signals / centroid policy`).
- `single_plate_control`: Single-plate control with IIIC process layer enabled. No plate-pair contacts exist, so process overlays should stay blank. (`10k / 1 plates / seed 42 / 1 rigid steps / zero_motion motion / default material / IIIC process on / slab pull off / expected no_process_signals / centroid policy`).
- `process_layer_default`: Consolidated IIIC process-layer fixture: marks, trench elevation split, and overriding uplift enabled; slab pull remains off. (`10k / 2 plates / seed 42 / 1 rigid steps / forced_convergence motion / mixed_plate0_continental material / IIIC process on / slab pull off / expected process_signals_required / centroid policy`).
- `default_40_plate_process`: Default 40-plate spatial sanity run with the consolidated IIIC process layer enabled and slab pull off. This is a human-inspection map, not a hard morphology gate. (`60k / 40 plates / seed 42 / 40 rigid steps / default motion / default material / IIIC process on / slab pull off / expected informational / centroid policy`).

## Gate Summary

| Gate | Result | Evidence |
|---|---:|---|
| map exports written | pass | output root `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z` |
| `zero_motion_control` export read-only | pass | replay 0 `unchanged hashes`, replay 1 `unchanged hashes` |
| `zero_motion_control` same-seed map hashes | pass | replay hashes byte-identical per layer |
| `zero_motion_control` expected spatial signal | pass | expected `no_process_signals` |
| `single_plate_control` export read-only | pass | replay 0 `unchanged hashes`, replay 1 `unchanged hashes` |
| `single_plate_control` same-seed map hashes | pass | replay hashes byte-identical per layer |
| `single_plate_control` expected spatial signal | pass | expected `no_process_signals` |
| `process_layer_default` export read-only | pass | replay 0 `unchanged hashes`, replay 1 `unchanged hashes` |
| `process_layer_default` same-seed map hashes | pass | replay hashes byte-identical per layer |
| `process_layer_default` expected spatial signal | pass | expected `process_signals_required` |
| `default_40_plate_process` export read-only | pass | replay 0 `unchanged hashes`, replay 1 `unchanged hashes` |
| `default_40_plate_process` same-seed map hashes | pass | replay hashes byte-identical per layer |
| `default_40_plate_process` expected spatial signal | pass | expected `informational` |

## Tracking State Used For Maps

| Scenario | Replay | Step | Active triangles | Distance records | Matrix pairs | Polarity decisions | Subduction polarity | Propagation seeds | Propagation added | Convergence hash |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| `zero_motion_control` | 0 | 1 | 362 | 362 | 0 | 0 | 0 | 0 | 0 | `2bd0e26181fad2fe` |
| `zero_motion_control` | 1 | 1 | 362 | 362 | 0 | 0 | 0 | 0 | 0 | `2bd0e26181fad2fe` |
| `single_plate_control` | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | `02025b45d9aae8fa` |
| `single_plate_control` | 1 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | `02025b45d9aae8fa` |
| `process_layer_default` | 0 | 1 | 446 | 446 | 1 | 1 | 1 | 84 | 84 | `51f1c267444ff160` |
| `process_layer_default` | 1 | 1 | 446 | 446 | 1 | 1 | 1 | 84 | 84 | `51f1c267444ff160` |
| `default_40_plate_process` | 0 | 40 | 0 | 0 | 75 | 75 | 17 | 0 | 0 | `d145f51eab69d5d8` |
| `default_40_plate_process` | 1 | 40 | 0 | 0 | 75 | 75 | 17 | 0 | 0 | `d145f51eab69d5d8` |

## IIIC State Used For Maps

| Scenario | Replay | Process layer | Slab pull | Marks | Trench records | Uplift records | Ledger records | Actual delta km | Ledger residual km | Mark hash | Elevation hash | Uplift hash | Ledger hash |
|---|---:|---|---|---:|---:|---:|---:|---:|---:|---|---|---|---|
| `zero_motion_control` | 0 | on | off | 0 | 0 | 0 | 0 | 0.000000000000 | 0.000000000000e+00 | `c3b2d96779aa2587` | `d73408457d40f43a` | `c941c3735bf40d22` | `7395c67e19f557a2` |
| `zero_motion_control` | 1 | on | off | 0 | 0 | 0 | 0 | 0.000000000000 | 0.000000000000e+00 | `c3b2d96779aa2587` | `d73408457d40f43a` | `c941c3735bf40d22` | `7395c67e19f557a2` |
| `single_plate_control` | 0 | on | off | 0 | 0 | 0 | 0 | 0.000000000000 | 0.000000000000e+00 | `c3b2d96779aa2587` | `a84052afa5482db5` | `c941c3735bf40d22` | `86ca96aafac805a2` |
| `single_plate_control` | 1 | on | off | 0 | 0 | 0 | 0 | 0.000000000000 | 0.000000000000e+00 | `c3b2d96779aa2587` | `a84052afa5482db5` | `c941c3735bf40d22` | `86ca96aafac805a2` |
| `process_layer_default` | 0 | on | off | 84 | 149 | 11040 | 11189 | -51.176588935012 | 1.854516540334e-12 | `d32e7289d548e202` | `a5bbbb2372fed5d1` | `74b54dd1f052ced4` | `a27dee465fa17ffb` |
| `process_layer_default` | 1 | on | off | 84 | 149 | 11040 | 11189 | -51.176588935012 | 1.854516540334e-12 | `d32e7289d548e202` | `a5bbbb2372fed5d1` | `74b54dd1f052ced4` | `a27dee465fa17ffb` |
| `default_40_plate_process` | 0 | on | off | 2572 | 0 | 252289 | 252289 | 7885.039858736331 | -9.090399544220e-09 | `83fbba7eb4e5064a` | `ee7f353cea6fb22c` | `db03453d0a9b4fc2` | `b0695e68f9a09bd2` |
| `default_40_plate_process` | 1 | on | off | 2572 | 0 | 252289 | 252289 | 7885.039858736331 | -9.090399544220e-09 | `83fbba7eb4e5064a` | `ee7f353cea6fb22c` | `db03453d0a9b4fc2` | `b0695e68f9a09bd2` |

## Exported Maps

| Scenario | Replay | Layer | Hash | Non-background pixels | Path |
|---|---:|---|---|---:|---|
| `zero_motion_control` | 0 | `PhaseIIISummary` | `9b937ddd87c693f2` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/zero_motion_control/replay_0/PhaseIIISummary.png` |
| `zero_motion_control` | 0 | `ElevationHeatmap` | `fc7eae57dda1b8cd` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/zero_motion_control/replay_0/ElevationHeatmap.png` |
| `zero_motion_control` | 0 | `SubductionMask` | `75a4704ad8bead02` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/zero_motion_control/replay_0/SubductionMask.png` |
| `zero_motion_control` | 0 | `DistanceToFrontHeatmap` | `b7912ba78237df67` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/zero_motion_control/replay_0/DistanceToFrontHeatmap.png` |
| `zero_motion_control` | 0 | `ElevationProfile` | `041955de1747fc4a` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/zero_motion_control/replay_0/ElevationProfile.png` |
| `zero_motion_control` | 0 | `ContactSheet` | n/a | n/a | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/zero_motion_control/replay_0/ContactSheet.png` |
| `zero_motion_control` | 1 | `PhaseIIISummary` | `9b937ddd87c693f2` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/zero_motion_control/replay_1/PhaseIIISummary.png` |
| `zero_motion_control` | 1 | `ElevationHeatmap` | `fc7eae57dda1b8cd` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/zero_motion_control/replay_1/ElevationHeatmap.png` |
| `zero_motion_control` | 1 | `SubductionMask` | `75a4704ad8bead02` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/zero_motion_control/replay_1/SubductionMask.png` |
| `zero_motion_control` | 1 | `DistanceToFrontHeatmap` | `b7912ba78237df67` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/zero_motion_control/replay_1/DistanceToFrontHeatmap.png` |
| `zero_motion_control` | 1 | `ElevationProfile` | `041955de1747fc4a` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/zero_motion_control/replay_1/ElevationProfile.png` |
| `zero_motion_control` | 1 | `ContactSheet` | n/a | n/a | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/zero_motion_control/replay_1/ContactSheet.png` |
| `single_plate_control` | 0 | `PhaseIIISummary` | `070f1348d892a27a` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/single_plate_control/replay_0/PhaseIIISummary.png` |
| `single_plate_control` | 0 | `ElevationHeatmap` | `070f1348d892a27a` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/single_plate_control/replay_0/ElevationHeatmap.png` |
| `single_plate_control` | 0 | `SubductionMask` | `863b6fa0e24c7b5a` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/single_plate_control/replay_0/SubductionMask.png` |
| `single_plate_control` | 0 | `DistanceToFrontHeatmap` | `863b6fa0e24c7b5a` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/single_plate_control/replay_0/DistanceToFrontHeatmap.png` |
| `single_plate_control` | 0 | `ElevationProfile` | `041955de1747fc4a` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/single_plate_control/replay_0/ElevationProfile.png` |
| `single_plate_control` | 0 | `ContactSheet` | n/a | n/a | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/single_plate_control/replay_0/ContactSheet.png` |
| `single_plate_control` | 1 | `PhaseIIISummary` | `070f1348d892a27a` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/single_plate_control/replay_1/PhaseIIISummary.png` |
| `single_plate_control` | 1 | `ElevationHeatmap` | `070f1348d892a27a` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/single_plate_control/replay_1/ElevationHeatmap.png` |
| `single_plate_control` | 1 | `SubductionMask` | `863b6fa0e24c7b5a` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/single_plate_control/replay_1/SubductionMask.png` |
| `single_plate_control` | 1 | `DistanceToFrontHeatmap` | `863b6fa0e24c7b5a` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/single_plate_control/replay_1/DistanceToFrontHeatmap.png` |
| `single_plate_control` | 1 | `ElevationProfile` | `041955de1747fc4a` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/single_plate_control/replay_1/ElevationProfile.png` |
| `single_plate_control` | 1 | `ContactSheet` | n/a | n/a | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/single_plate_control/replay_1/ContactSheet.png` |
| `process_layer_default` | 0 | `PhaseIIISummary` | `42d8587506490d48` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/process_layer_default/replay_0/PhaseIIISummary.png` |
| `process_layer_default` | 0 | `ElevationHeatmap` | `0b2f725974cce797` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/process_layer_default/replay_0/ElevationHeatmap.png` |
| `process_layer_default` | 0 | `SubductionMask` | `fe412a294f3e908c` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/process_layer_default/replay_0/SubductionMask.png` |
| `process_layer_default` | 0 | `DistanceToFrontHeatmap` | `d0d93f5f6dca6e33` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/process_layer_default/replay_0/DistanceToFrontHeatmap.png` |
| `process_layer_default` | 0 | `ElevationProfile` | `f2b2a64486870e5b` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/process_layer_default/replay_0/ElevationProfile.png` |
| `process_layer_default` | 0 | `ContactSheet` | n/a | n/a | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/process_layer_default/replay_0/ContactSheet.png` |
| `process_layer_default` | 1 | `PhaseIIISummary` | `42d8587506490d48` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/process_layer_default/replay_1/PhaseIIISummary.png` |
| `process_layer_default` | 1 | `ElevationHeatmap` | `0b2f725974cce797` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/process_layer_default/replay_1/ElevationHeatmap.png` |
| `process_layer_default` | 1 | `SubductionMask` | `fe412a294f3e908c` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/process_layer_default/replay_1/SubductionMask.png` |
| `process_layer_default` | 1 | `DistanceToFrontHeatmap` | `d0d93f5f6dca6e33` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/process_layer_default/replay_1/DistanceToFrontHeatmap.png` |
| `process_layer_default` | 1 | `ElevationProfile` | `f2b2a64486870e5b` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/process_layer_default/replay_1/ElevationProfile.png` |
| `process_layer_default` | 1 | `ContactSheet` | n/a | n/a | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/process_layer_default/replay_1/ContactSheet.png` |
| `default_40_plate_process` | 0 | `PhaseIIISummary` | `bb2f00fa2ce53be9` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/default_40_plate_process/replay_0/PhaseIIISummary.png` |
| `default_40_plate_process` | 0 | `ElevationHeatmap` | `8511c27ddffa3783` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/default_40_plate_process/replay_0/ElevationHeatmap.png` |
| `default_40_plate_process` | 0 | `SubductionMask` | `7237137d5152a581` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/default_40_plate_process/replay_0/SubductionMask.png` |
| `default_40_plate_process` | 0 | `DistanceToFrontHeatmap` | `9bce3bfd71b26341` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/default_40_plate_process/replay_0/DistanceToFrontHeatmap.png` |
| `default_40_plate_process` | 0 | `ElevationProfile` | `bf68479c2b379589` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/default_40_plate_process/replay_0/ElevationProfile.png` |
| `default_40_plate_process` | 0 | `ContactSheet` | n/a | n/a | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/default_40_plate_process/replay_0/ContactSheet.png` |
| `default_40_plate_process` | 1 | `PhaseIIISummary` | `bb2f00fa2ce53be9` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/default_40_plate_process/replay_1/PhaseIIISummary.png` |
| `default_40_plate_process` | 1 | `ElevationHeatmap` | `8511c27ddffa3783` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/default_40_plate_process/replay_1/ElevationHeatmap.png` |
| `default_40_plate_process` | 1 | `SubductionMask` | `7237137d5152a581` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/default_40_plate_process/replay_1/SubductionMask.png` |
| `default_40_plate_process` | 1 | `DistanceToFrontHeatmap` | `9bce3bfd71b26341` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/default_40_plate_process/replay_1/DistanceToFrontHeatmap.png` |
| `default_40_plate_process` | 1 | `ElevationProfile` | `bf68479c2b379589` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/default_40_plate_process/replay_1/ElevationProfile.png` |
| `default_40_plate_process` | 1 | `ContactSheet` | n/a | n/a | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T010705Z/default_40_plate_process/replay_1/ContactSheet.png` |

## Interpretation

- `PhaseIIISummary` is the human-inspection layer: filled crust type, plate-boundary emphasis, velocity arrows in PNG exports, IIIB distance context, IIIC subduction roles, and IIIC elevation overlays in one map. It is still a color diagnostic on a unit sphere, not terrain displacement.
- `ElevationHeatmap` should show the IIIC.2 trench split and IIIC.3 overriding uplift as scalar-field color overlays on the filled continental/oceanic base map.
- `SubductionMask` should show the consolidated IIIC subducting/overriding roles produced from persistent plate-local marks, not only the earlier pre-mutation IIIB inspection overlay.
- `DistanceToFrontHeatmap` remains the front-distance spatial context for the same fixture; it is diagnostic context, not a source of authority.

- `ElevationProfile` plots uplift delta against distance-to-front and includes the expected thesis distance-transfer curve as a visual shape reference. It is paired with the IIIC.3 numeric oracle; the plot alone is not a gate.

## Recommendation

IIIC map export passes. These images are suitable human spatial sanity artifacts for the consolidated IIIC process layer before Phase IIID work begins.
