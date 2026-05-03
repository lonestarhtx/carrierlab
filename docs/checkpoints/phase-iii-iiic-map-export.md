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
| map exports written | pass | output root `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z` |
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
| `zero_motion_control` | 0 | `CrustType` | `81e161027ddbdd36` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/zero_motion_control/replay_0/CrustType.png` |
| `zero_motion_control` | 0 | `PlateBoundaries` | `92cb023a82f9b5d4` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/zero_motion_control/replay_0/PlateBoundaries.png` |
| `zero_motion_control` | 0 | `VelocityField` | `81e161027ddbdd36` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/zero_motion_control/replay_0/VelocityField.png` |
| `zero_motion_control` | 0 | `SubductionRoles` | `a1c7057ffd4e9b38` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/zero_motion_control/replay_0/SubductionRoles.png` |
| `zero_motion_control` | 0 | `Elevation` | `10d8f130e1c57888` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/zero_motion_control/replay_0/Elevation.png` |
| `zero_motion_control` | 0 | `CombinedTectonicSummary` | `92cb023a82f9b5d4` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/zero_motion_control/replay_0/CombinedTectonicSummary.png` |
| `zero_motion_control` | 0 | `DistanceToFront` | `5cdc5f9708d1bf7a` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/zero_motion_control/replay_0/DistanceToFront.png` |
| `zero_motion_control` | 0 | `ElevationProfile` | `041955de1747fc4a` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/zero_motion_control/replay_0/ElevationProfile.png` |
| `zero_motion_control` | 0 | `ContactSheet` | n/a | n/a | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/zero_motion_control/replay_0/ContactSheet.png` |
| `zero_motion_control` | 1 | `CrustType` | `81e161027ddbdd36` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/zero_motion_control/replay_1/CrustType.png` |
| `zero_motion_control` | 1 | `PlateBoundaries` | `92cb023a82f9b5d4` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/zero_motion_control/replay_1/PlateBoundaries.png` |
| `zero_motion_control` | 1 | `VelocityField` | `81e161027ddbdd36` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/zero_motion_control/replay_1/VelocityField.png` |
| `zero_motion_control` | 1 | `SubductionRoles` | `a1c7057ffd4e9b38` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/zero_motion_control/replay_1/SubductionRoles.png` |
| `zero_motion_control` | 1 | `Elevation` | `10d8f130e1c57888` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/zero_motion_control/replay_1/Elevation.png` |
| `zero_motion_control` | 1 | `CombinedTectonicSummary` | `92cb023a82f9b5d4` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/zero_motion_control/replay_1/CombinedTectonicSummary.png` |
| `zero_motion_control` | 1 | `DistanceToFront` | `5cdc5f9708d1bf7a` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/zero_motion_control/replay_1/DistanceToFront.png` |
| `zero_motion_control` | 1 | `ElevationProfile` | `041955de1747fc4a` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/zero_motion_control/replay_1/ElevationProfile.png` |
| `zero_motion_control` | 1 | `ContactSheet` | n/a | n/a | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/zero_motion_control/replay_1/ContactSheet.png` |
| `single_plate_control` | 0 | `CrustType` | `259a365d0028cc06` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/single_plate_control/replay_0/CrustType.png` |
| `single_plate_control` | 0 | `PlateBoundaries` | `259a365d0028cc06` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/single_plate_control/replay_0/PlateBoundaries.png` |
| `single_plate_control` | 0 | `VelocityField` | `259a365d0028cc06` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/single_plate_control/replay_0/VelocityField.png` |
| `single_plate_control` | 0 | `SubductionRoles` | `2ac040d556514886` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/single_plate_control/replay_0/SubductionRoles.png` |
| `single_plate_control` | 0 | `Elevation` | `259a365d0028cc06` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/single_plate_control/replay_0/Elevation.png` |
| `single_plate_control` | 0 | `CombinedTectonicSummary` | `259a365d0028cc06` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/single_plate_control/replay_0/CombinedTectonicSummary.png` |
| `single_plate_control` | 0 | `DistanceToFront` | `2ac040d556514886` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/single_plate_control/replay_0/DistanceToFront.png` |
| `single_plate_control` | 0 | `ElevationProfile` | `041955de1747fc4a` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/single_plate_control/replay_0/ElevationProfile.png` |
| `single_plate_control` | 0 | `ContactSheet` | n/a | n/a | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/single_plate_control/replay_0/ContactSheet.png` |
| `single_plate_control` | 1 | `CrustType` | `259a365d0028cc06` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/single_plate_control/replay_1/CrustType.png` |
| `single_plate_control` | 1 | `PlateBoundaries` | `259a365d0028cc06` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/single_plate_control/replay_1/PlateBoundaries.png` |
| `single_plate_control` | 1 | `VelocityField` | `259a365d0028cc06` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/single_plate_control/replay_1/VelocityField.png` |
| `single_plate_control` | 1 | `SubductionRoles` | `2ac040d556514886` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/single_plate_control/replay_1/SubductionRoles.png` |
| `single_plate_control` | 1 | `Elevation` | `259a365d0028cc06` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/single_plate_control/replay_1/Elevation.png` |
| `single_plate_control` | 1 | `CombinedTectonicSummary` | `259a365d0028cc06` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/single_plate_control/replay_1/CombinedTectonicSummary.png` |
| `single_plate_control` | 1 | `DistanceToFront` | `2ac040d556514886` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/single_plate_control/replay_1/DistanceToFront.png` |
| `single_plate_control` | 1 | `ElevationProfile` | `041955de1747fc4a` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/single_plate_control/replay_1/ElevationProfile.png` |
| `single_plate_control` | 1 | `ContactSheet` | n/a | n/a | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/single_plate_control/replay_1/ContactSheet.png` |
| `process_layer_default` | 0 | `CrustType` | `d7107b237e34c9a6` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/process_layer_default/replay_0/CrustType.png` |
| `process_layer_default` | 0 | `PlateBoundaries` | `339f69a71769933b` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/process_layer_default/replay_0/PlateBoundaries.png` |
| `process_layer_default` | 0 | `VelocityField` | `0491aab7bf8b59b6` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/process_layer_default/replay_0/VelocityField.png` |
| `process_layer_default` | 0 | `SubductionRoles` | `6da79499cfec8923` | 1647143 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/process_layer_default/replay_0/SubductionRoles.png` |
| `process_layer_default` | 0 | `Elevation` | `f3d258ca47ac1a66` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/process_layer_default/replay_0/Elevation.png` |
| `process_layer_default` | 0 | `CombinedTectonicSummary` | `9e021dc374e4645e` | 1647143 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/process_layer_default/replay_0/CombinedTectonicSummary.png` |
| `process_layer_default` | 0 | `DistanceToFront` | `818f54e75e3b9d1a` | 1647146 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/process_layer_default/replay_0/DistanceToFront.png` |
| `process_layer_default` | 0 | `ElevationProfile` | `f2b2a64486870e5b` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/process_layer_default/replay_0/ElevationProfile.png` |
| `process_layer_default` | 0 | `ContactSheet` | n/a | n/a | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/process_layer_default/replay_0/ContactSheet.png` |
| `process_layer_default` | 1 | `CrustType` | `d7107b237e34c9a6` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/process_layer_default/replay_1/CrustType.png` |
| `process_layer_default` | 1 | `PlateBoundaries` | `339f69a71769933b` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/process_layer_default/replay_1/PlateBoundaries.png` |
| `process_layer_default` | 1 | `VelocityField` | `0491aab7bf8b59b6` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/process_layer_default/replay_1/VelocityField.png` |
| `process_layer_default` | 1 | `SubductionRoles` | `6da79499cfec8923` | 1647143 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/process_layer_default/replay_1/SubductionRoles.png` |
| `process_layer_default` | 1 | `Elevation` | `f3d258ca47ac1a66` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/process_layer_default/replay_1/Elevation.png` |
| `process_layer_default` | 1 | `CombinedTectonicSummary` | `9e021dc374e4645e` | 1647143 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/process_layer_default/replay_1/CombinedTectonicSummary.png` |
| `process_layer_default` | 1 | `DistanceToFront` | `818f54e75e3b9d1a` | 1647146 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/process_layer_default/replay_1/DistanceToFront.png` |
| `process_layer_default` | 1 | `ElevationProfile` | `f2b2a64486870e5b` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/process_layer_default/replay_1/ElevationProfile.png` |
| `process_layer_default` | 1 | `ContactSheet` | n/a | n/a | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/process_layer_default/replay_1/ContactSheet.png` |
| `default_40_plate_process` | 0 | `CrustType` | `c07bedb84f785b5b` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/default_40_plate_process/replay_0/CrustType.png` |
| `default_40_plate_process` | 0 | `PlateBoundaries` | `abdfcd1cd3289f78` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/default_40_plate_process/replay_0/PlateBoundaries.png` |
| `default_40_plate_process` | 0 | `VelocityField` | `8709ab48fe48dd43` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/default_40_plate_process/replay_0/VelocityField.png` |
| `default_40_plate_process` | 0 | `SubductionRoles` | `cd09ac57d08eaa55` | 1647156 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/default_40_plate_process/replay_0/SubductionRoles.png` |
| `default_40_plate_process` | 0 | `Elevation` | `9c39446e4b2fd049` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/default_40_plate_process/replay_0/Elevation.png` |
| `default_40_plate_process` | 0 | `CombinedTectonicSummary` | `87ca56e7afc5b119` | 1647156 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/default_40_plate_process/replay_0/CombinedTectonicSummary.png` |
| `default_40_plate_process` | 0 | `DistanceToFront` | `46e2116e7b6ff9f1` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/default_40_plate_process/replay_0/DistanceToFront.png` |
| `default_40_plate_process` | 0 | `ElevationProfile` | `bf68479c2b379589` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/default_40_plate_process/replay_0/ElevationProfile.png` |
| `default_40_plate_process` | 0 | `ContactSheet` | n/a | n/a | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/default_40_plate_process/replay_0/ContactSheet.png` |
| `default_40_plate_process` | 1 | `CrustType` | `c07bedb84f785b5b` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/default_40_plate_process/replay_1/CrustType.png` |
| `default_40_plate_process` | 1 | `PlateBoundaries` | `abdfcd1cd3289f78` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/default_40_plate_process/replay_1/PlateBoundaries.png` |
| `default_40_plate_process` | 1 | `VelocityField` | `8709ab48fe48dd43` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/default_40_plate_process/replay_1/VelocityField.png` |
| `default_40_plate_process` | 1 | `SubductionRoles` | `cd09ac57d08eaa55` | 1647156 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/default_40_plate_process/replay_1/SubductionRoles.png` |
| `default_40_plate_process` | 1 | `Elevation` | `9c39446e4b2fd049` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/default_40_plate_process/replay_1/Elevation.png` |
| `default_40_plate_process` | 1 | `CombinedTectonicSummary` | `87ca56e7afc5b119` | 1647156 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/default_40_plate_process/replay_1/CombinedTectonicSummary.png` |
| `default_40_plate_process` | 1 | `DistanceToFront` | `46e2116e7b6ff9f1` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/default_40_plate_process/replay_1/DistanceToFront.png` |
| `default_40_plate_process` | 1 | `ElevationProfile` | `bf68479c2b379589` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/default_40_plate_process/replay_1/ElevationProfile.png` |
| `default_40_plate_process` | 1 | `ContactSheet` | n/a | n/a | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T013935Z/default_40_plate_process/replay_1/ContactSheet.png` |

## Interpretation

- The export names now follow the Aurous-style diagnostic grammar: `CrustType`, `PlateBoundaries`, `VelocityField`, `SubductionRoles`, `Elevation`, `CombinedTectonicSummary`, `DistanceToFront`, and `ElevationProfile`.
- `CrustType` is the calm base map: blue ocean, green land, no process overlay.
- `PlateBoundaries` adds only thin light boundary evidence over the same base. This is the first map to check when the summary feels noisy.
- `VelocityField` adds sparse red plate-motion arrows over the base map.
- `SubductionRoles` is now rasterized directly from current plate-local triangle geometry. It no longer round-trips through `GlobalSampleId`, so the moving role marks should line up with the current moving plate geometry.
- `Elevation` shows the IIIC.2 trench split and IIIC.3 overriding uplift as scalar-field color on the filled continental/oceanic base map.
- `CombinedTectonicSummary` is deliberately restrained: crust + boundaries + velocity + subduction roles. Elevation remains separate so uplift heat does not swamp the overview.
- `DistanceToFront` is also rasterized from current plate-local active-boundary triangles. It is diagnostic context, not a source of authority.
- Contact sheet headers include the simulation step and approximate `Myr` timestamp using the CarrierLab `2 Ma` timestep.
- `ElevationProfile` plots uplift delta against distance-to-front and includes the expected thesis distance-transfer curve as a visual shape reference. It is paired with the IIIC.3 numeric oracle; the plot alone is not a gate.

## Recommendation

IIIC map export passes. These images are suitable human spatial sanity artifacts for the consolidated IIIC process layer before Phase IIID work begins; they remain read-only evidence maps, not terrain morphology reproduction.
