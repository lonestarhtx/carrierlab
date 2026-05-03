# Phase III IIIC Map Export Checkpoint

Status: read-only spatial sanity export for the consolidated IIIC process layer.

## Scope

This checkpoint exports filled Mollweide-style PNG artifacts after the consolidated IIIC process layer has run with slab pull off. The export path itself is read-only. Some fixtures explicitly enable observed-speed natural resampling before export; those events are reported as cadence evidence, not hidden visualization cleanup. When IIIC subducting marks are enabled, natural cadence events call the existing filtered resampling path so marked subducting triangles are excluded rather than silently ignored. The export path must not add unreported process mutation, triangle consumption, material transfer, forbidden authority fallback patterns, or projection-derived carrier authority.

Fixtures:

- `zero_motion_control`: Zero-motion control with IIIC process layer enabled. It should remain spatially blank for subduction, trench, and uplift signals. (`10k / 2 plates / seed 42 / 1 steps / zero_motion motion / mixed_plate0_continental material / natural resampling off / IIIC process on / slab pull off / expected no_process_signals / centroid policy`).
- `single_plate_control`: Single-plate control with IIIC process layer enabled. No plate-pair contacts exist, so process overlays should stay blank. (`10k / 1 plates / seed 42 / 1 steps / zero_motion motion / default material / natural resampling off / IIIC process on / slab pull off / expected no_process_signals / centroid policy`).
- `process_layer_default`: Consolidated IIIC process-layer fixture: marks, trench elevation split, and overriding uplift enabled; slab pull remains off. (`10k / 2 plates / seed 42 / 1 steps / forced_convergence motion / mixed_plate0_continental material / natural resampling off / IIIC process on / slab pull off / expected process_signals_required / centroid policy`).
- `default_40_plate_process`: Default 40-plate rigid-window spatial sanity run with the consolidated IIIC process layer enabled, natural resampling disabled, and slab pull off. This preserves the pre-cadence map baseline. (`60k / 40 plates / seed 42 / 40 steps / default motion / default material / natural resampling off / IIIC process on / slab pull off / expected informational / centroid policy`).
- `default_40_plate_process_cadence`: Default 40-plate spatial sanity run with the consolidated IIIC process layer enabled, slab pull off, and observed-speed natural resampling enabled. This exercises cadence firing through the actor path; with IIIC marks enabled, natural events use the filtered resampling path. (`60k / 40 plates / seed 42 / 40 steps / default motion / default material / natural resampling on / IIIC process on / slab pull off / expected informational / centroid policy`).

## Gate Summary

| Gate | Result | Evidence |
|---|---:|---|
| map exports written | pass | output root `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z` |
| `zero_motion_control` export read-only | pass | replay 0 `unchanged hashes`, replay 1 `unchanged hashes` |
| `zero_motion_control` same-seed map hashes | pass | replay hashes byte-identical per layer |
| `zero_motion_control` expected spatial signal | pass | expected `no_process_signals` |
| `zero_motion_control` cadence behavior | pass | natural resampling `off`, events 0/0, cadence 64 steps / 128.000 Ma, observed vmax 0.000000 mm/yr |
| `single_plate_control` export read-only | pass | replay 0 `unchanged hashes`, replay 1 `unchanged hashes` |
| `single_plate_control` same-seed map hashes | pass | replay hashes byte-identical per layer |
| `single_plate_control` expected spatial signal | pass | expected `no_process_signals` |
| `single_plate_control` cadence behavior | pass | natural resampling `off`, events 0/0, cadence 64 steps / 128.000 Ma, observed vmax 0.000000 mm/yr |
| `process_layer_default` export read-only | pass | replay 0 `unchanged hashes`, replay 1 `unchanged hashes` |
| `process_layer_default` same-seed map hashes | pass | replay hashes byte-identical per layer |
| `process_layer_default` expected spatial signal | pass | expected `process_signals_required` |
| `process_layer_default` cadence behavior | pass | natural resampling `off`, events 0/0, cadence 16 steps / 32.000 Ma, observed vmax 120.000000 mm/yr |
| `default_40_plate_process` export read-only | pass | replay 0 `unchanged hashes`, replay 1 `unchanged hashes` |
| `default_40_plate_process` same-seed map hashes | pass | replay hashes byte-identical per layer |
| `default_40_plate_process` expected spatial signal | pass | expected `informational` |
| `default_40_plate_process` cadence behavior | pass | natural resampling `off`, events 0/0, cadence 32 steps / 64.000 Ma, observed vmax 66.666667 mm/yr |
| `default_40_plate_process_cadence` export read-only | pass | replay 0 `unchanged hashes`, replay 1 `unchanged hashes` |
| `default_40_plate_process_cadence` same-seed map hashes | pass | replay hashes byte-identical per layer |
| `default_40_plate_process_cadence` expected spatial signal | pass | expected `informational` |
| `default_40_plate_process_cadence` cadence behavior | pass | natural resampling `on`, events 1/1, cadence 32 steps / 64.000 Ma, observed vmax 66.666667 mm/yr |

## Cadence State

| Scenario | Replay | Step | Events | Next resample | Cadence steps | DeltaT Ma | Observed max speed mm/yr | Reset serial |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `zero_motion_control` | 0 | 1 | 0 | 64 | 64 | 128.000000 | 0.000000000 | 0 |
| `zero_motion_control` | 1 | 1 | 0 | 64 | 64 | 128.000000 | 0.000000000 | 0 |
| `single_plate_control` | 0 | 1 | 0 | 64 | 64 | 128.000000 | 0.000000000 | 0 |
| `single_plate_control` | 1 | 1 | 0 | 64 | 64 | 128.000000 | 0.000000000 | 0 |
| `process_layer_default` | 0 | 1 | 0 | 16 | 16 | 32.000000 | 120.000000000 | 0 |
| `process_layer_default` | 1 | 1 | 0 | 16 | 16 | 32.000000 | 120.000000000 | 0 |
| `default_40_plate_process` | 0 | 40 | 0 | 64 | 32 | 64.000000 | 66.666666667 | 0 |
| `default_40_plate_process` | 1 | 40 | 0 | 64 | 32 | 64.000000 | 66.666666667 | 0 |
| `default_40_plate_process_cadence` | 0 | 40 | 1 | 64 | 32 | 64.000000 | 66.666666667 | 1 |
| `default_40_plate_process_cadence` | 1 | 40 | 1 | 64 | 32 | 64.000000 | 66.666666667 | 1 |

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
| `default_40_plate_process_cadence` | 0 | 40 | 12955 | 12955 | 140 | 140 | 50 | 2119 | 342 | `4c33a8b319b073f4` |
| `default_40_plate_process_cadence` | 1 | 40 | 12955 | 12955 | 140 | 140 | 50 | 2119 | 342 | `4c33a8b319b073f4` |

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
| `default_40_plate_process_cadence` | 0 | on | off | 3177 | 274 | 1253411 | 1253685 | 103438.285815773881 | 7.916241884232e-09 | `53b234bbdfe2edaa` | `a5215be82b7401f5` | `426bfcab11cd08e1` | `868f3ec716f32931` |
| `default_40_plate_process_cadence` | 1 | on | off | 3177 | 274 | 1253411 | 1253685 | 103438.285815773881 | 7.916241884232e-09 | `53b234bbdfe2edaa` | `a5215be82b7401f5` | `426bfcab11cd08e1` | `868f3ec716f32931` |

## Exported Maps

| Scenario | Replay | Layer | Hash | Non-background pixels | Path |
|---|---:|---|---|---:|---|
| `zero_motion_control` | 0 | `PlateId` | `5e19d3f40f8b1376` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/zero_motion_control/replay_0/PlateId.png` |
| `zero_motion_control` | 0 | `CrustType` | `81e161027ddbdd36` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/zero_motion_control/replay_0/CrustType.png` |
| `zero_motion_control` | 0 | `PlateBoundaries` | `e9bfe79bec0dfefa` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/zero_motion_control/replay_0/PlateBoundaries.png` |
| `zero_motion_control` | 0 | `VelocityField` | `81e161027ddbdd36` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/zero_motion_control/replay_0/VelocityField.png` |
| `zero_motion_control` | 0 | `SubductionRoles` | `a1c7057ffd4e9b38` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/zero_motion_control/replay_0/SubductionRoles.png` |
| `zero_motion_control` | 0 | `Elevation` | `10d8f130e1c57888` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/zero_motion_control/replay_0/Elevation.png` |
| `zero_motion_control` | 0 | `CombinedTectonicSummary` | `e9bfe79bec0dfefa` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/zero_motion_control/replay_0/CombinedTectonicSummary.png` |
| `zero_motion_control` | 0 | `DistanceToFront` | `5cdc5f9708d1bf7a` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/zero_motion_control/replay_0/DistanceToFront.png` |
| `zero_motion_control` | 0 | `ElevationProfile` | `041955de1747fc4a` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/zero_motion_control/replay_0/ElevationProfile.png` |
| `zero_motion_control` | 0 | `ContactSheet` | n/a | n/a | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/zero_motion_control/replay_0/ContactSheet.png` |
| `zero_motion_control` | 1 | `PlateId` | `5e19d3f40f8b1376` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/zero_motion_control/replay_1/PlateId.png` |
| `zero_motion_control` | 1 | `CrustType` | `81e161027ddbdd36` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/zero_motion_control/replay_1/CrustType.png` |
| `zero_motion_control` | 1 | `PlateBoundaries` | `e9bfe79bec0dfefa` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/zero_motion_control/replay_1/PlateBoundaries.png` |
| `zero_motion_control` | 1 | `VelocityField` | `81e161027ddbdd36` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/zero_motion_control/replay_1/VelocityField.png` |
| `zero_motion_control` | 1 | `SubductionRoles` | `a1c7057ffd4e9b38` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/zero_motion_control/replay_1/SubductionRoles.png` |
| `zero_motion_control` | 1 | `Elevation` | `10d8f130e1c57888` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/zero_motion_control/replay_1/Elevation.png` |
| `zero_motion_control` | 1 | `CombinedTectonicSummary` | `e9bfe79bec0dfefa` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/zero_motion_control/replay_1/CombinedTectonicSummary.png` |
| `zero_motion_control` | 1 | `DistanceToFront` | `5cdc5f9708d1bf7a` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/zero_motion_control/replay_1/DistanceToFront.png` |
| `zero_motion_control` | 1 | `ElevationProfile` | `041955de1747fc4a` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/zero_motion_control/replay_1/ElevationProfile.png` |
| `zero_motion_control` | 1 | `ContactSheet` | n/a | n/a | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/zero_motion_control/replay_1/ContactSheet.png` |
| `single_plate_control` | 0 | `PlateId` | `f7c2822e7c4e5686` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/single_plate_control/replay_0/PlateId.png` |
| `single_plate_control` | 0 | `CrustType` | `259a365d0028cc06` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/single_plate_control/replay_0/CrustType.png` |
| `single_plate_control` | 0 | `PlateBoundaries` | `259a365d0028cc06` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/single_plate_control/replay_0/PlateBoundaries.png` |
| `single_plate_control` | 0 | `VelocityField` | `259a365d0028cc06` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/single_plate_control/replay_0/VelocityField.png` |
| `single_plate_control` | 0 | `SubductionRoles` | `2ac040d556514886` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/single_plate_control/replay_0/SubductionRoles.png` |
| `single_plate_control` | 0 | `Elevation` | `259a365d0028cc06` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/single_plate_control/replay_0/Elevation.png` |
| `single_plate_control` | 0 | `CombinedTectonicSummary` | `259a365d0028cc06` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/single_plate_control/replay_0/CombinedTectonicSummary.png` |
| `single_plate_control` | 0 | `DistanceToFront` | `2ac040d556514886` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/single_plate_control/replay_0/DistanceToFront.png` |
| `single_plate_control` | 0 | `ElevationProfile` | `041955de1747fc4a` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/single_plate_control/replay_0/ElevationProfile.png` |
| `single_plate_control` | 0 | `ContactSheet` | n/a | n/a | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/single_plate_control/replay_0/ContactSheet.png` |
| `single_plate_control` | 1 | `PlateId` | `f7c2822e7c4e5686` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/single_plate_control/replay_1/PlateId.png` |
| `single_plate_control` | 1 | `CrustType` | `259a365d0028cc06` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/single_plate_control/replay_1/CrustType.png` |
| `single_plate_control` | 1 | `PlateBoundaries` | `259a365d0028cc06` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/single_plate_control/replay_1/PlateBoundaries.png` |
| `single_plate_control` | 1 | `VelocityField` | `259a365d0028cc06` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/single_plate_control/replay_1/VelocityField.png` |
| `single_plate_control` | 1 | `SubductionRoles` | `2ac040d556514886` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/single_plate_control/replay_1/SubductionRoles.png` |
| `single_plate_control` | 1 | `Elevation` | `259a365d0028cc06` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/single_plate_control/replay_1/Elevation.png` |
| `single_plate_control` | 1 | `CombinedTectonicSummary` | `259a365d0028cc06` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/single_plate_control/replay_1/CombinedTectonicSummary.png` |
| `single_plate_control` | 1 | `DistanceToFront` | `2ac040d556514886` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/single_plate_control/replay_1/DistanceToFront.png` |
| `single_plate_control` | 1 | `ElevationProfile` | `041955de1747fc4a` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/single_plate_control/replay_1/ElevationProfile.png` |
| `single_plate_control` | 1 | `ContactSheet` | n/a | n/a | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/single_plate_control/replay_1/ContactSheet.png` |
| `process_layer_default` | 0 | `PlateId` | `31994e3fbce4ac30` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/process_layer_default/replay_0/PlateId.png` |
| `process_layer_default` | 0 | `CrustType` | `d7107b237e34c9a6` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/process_layer_default/replay_0/CrustType.png` |
| `process_layer_default` | 0 | `PlateBoundaries` | `87928b6ad37e0f90` | 1647143 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/process_layer_default/replay_0/PlateBoundaries.png` |
| `process_layer_default` | 0 | `VelocityField` | `0491aab7bf8b59b6` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/process_layer_default/replay_0/VelocityField.png` |
| `process_layer_default` | 0 | `SubductionRoles` | `6da79499cfec8923` | 1647143 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/process_layer_default/replay_0/SubductionRoles.png` |
| `process_layer_default` | 0 | `Elevation` | `f3d258ca47ac1a66` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/process_layer_default/replay_0/Elevation.png` |
| `process_layer_default` | 0 | `CombinedTectonicSummary` | `84f360bb66667617` | 1647143 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/process_layer_default/replay_0/CombinedTectonicSummary.png` |
| `process_layer_default` | 0 | `DistanceToFront` | `818f54e75e3b9d1a` | 1647146 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/process_layer_default/replay_0/DistanceToFront.png` |
| `process_layer_default` | 0 | `ElevationProfile` | `f2b2a64486870e5b` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/process_layer_default/replay_0/ElevationProfile.png` |
| `process_layer_default` | 0 | `ContactSheet` | n/a | n/a | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/process_layer_default/replay_0/ContactSheet.png` |
| `process_layer_default` | 1 | `PlateId` | `31994e3fbce4ac30` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/process_layer_default/replay_1/PlateId.png` |
| `process_layer_default` | 1 | `CrustType` | `d7107b237e34c9a6` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/process_layer_default/replay_1/CrustType.png` |
| `process_layer_default` | 1 | `PlateBoundaries` | `87928b6ad37e0f90` | 1647143 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/process_layer_default/replay_1/PlateBoundaries.png` |
| `process_layer_default` | 1 | `VelocityField` | `0491aab7bf8b59b6` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/process_layer_default/replay_1/VelocityField.png` |
| `process_layer_default` | 1 | `SubductionRoles` | `6da79499cfec8923` | 1647143 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/process_layer_default/replay_1/SubductionRoles.png` |
| `process_layer_default` | 1 | `Elevation` | `f3d258ca47ac1a66` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/process_layer_default/replay_1/Elevation.png` |
| `process_layer_default` | 1 | `CombinedTectonicSummary` | `84f360bb66667617` | 1647143 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/process_layer_default/replay_1/CombinedTectonicSummary.png` |
| `process_layer_default` | 1 | `DistanceToFront` | `818f54e75e3b9d1a` | 1647146 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/process_layer_default/replay_1/DistanceToFront.png` |
| `process_layer_default` | 1 | `ElevationProfile` | `f2b2a64486870e5b` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/process_layer_default/replay_1/ElevationProfile.png` |
| `process_layer_default` | 1 | `ContactSheet` | n/a | n/a | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/process_layer_default/replay_1/ContactSheet.png` |
| `default_40_plate_process` | 0 | `PlateId` | `8993a02cfd628697` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/default_40_plate_process/replay_0/PlateId.png` |
| `default_40_plate_process` | 0 | `CrustType` | `c07bedb84f785b5b` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/default_40_plate_process/replay_0/CrustType.png` |
| `default_40_plate_process` | 0 | `PlateBoundaries` | `72795d9a59b0ae2b` | 1647186 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/default_40_plate_process/replay_0/PlateBoundaries.png` |
| `default_40_plate_process` | 0 | `VelocityField` | `8709ab48fe48dd43` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/default_40_plate_process/replay_0/VelocityField.png` |
| `default_40_plate_process` | 0 | `SubductionRoles` | `cd09ac57d08eaa55` | 1647156 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/default_40_plate_process/replay_0/SubductionRoles.png` |
| `default_40_plate_process` | 0 | `Elevation` | `9c39446e4b2fd049` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/default_40_plate_process/replay_0/Elevation.png` |
| `default_40_plate_process` | 0 | `CombinedTectonicSummary` | `044d1588da54d692` | 1647201 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/default_40_plate_process/replay_0/CombinedTectonicSummary.png` |
| `default_40_plate_process` | 0 | `DistanceToFront` | `46e2116e7b6ff9f1` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/default_40_plate_process/replay_0/DistanceToFront.png` |
| `default_40_plate_process` | 0 | `ElevationProfile` | `bf68479c2b379589` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/default_40_plate_process/replay_0/ElevationProfile.png` |
| `default_40_plate_process` | 0 | `ContactSheet` | n/a | n/a | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/default_40_plate_process/replay_0/ContactSheet.png` |
| `default_40_plate_process` | 1 | `PlateId` | `8993a02cfd628697` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/default_40_plate_process/replay_1/PlateId.png` |
| `default_40_plate_process` | 1 | `CrustType` | `c07bedb84f785b5b` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/default_40_plate_process/replay_1/CrustType.png` |
| `default_40_plate_process` | 1 | `PlateBoundaries` | `72795d9a59b0ae2b` | 1647186 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/default_40_plate_process/replay_1/PlateBoundaries.png` |
| `default_40_plate_process` | 1 | `VelocityField` | `8709ab48fe48dd43` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/default_40_plate_process/replay_1/VelocityField.png` |
| `default_40_plate_process` | 1 | `SubductionRoles` | `cd09ac57d08eaa55` | 1647156 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/default_40_plate_process/replay_1/SubductionRoles.png` |
| `default_40_plate_process` | 1 | `Elevation` | `9c39446e4b2fd049` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/default_40_plate_process/replay_1/Elevation.png` |
| `default_40_plate_process` | 1 | `CombinedTectonicSummary` | `044d1588da54d692` | 1647201 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/default_40_plate_process/replay_1/CombinedTectonicSummary.png` |
| `default_40_plate_process` | 1 | `DistanceToFront` | `46e2116e7b6ff9f1` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/default_40_plate_process/replay_1/DistanceToFront.png` |
| `default_40_plate_process` | 1 | `ElevationProfile` | `bf68479c2b379589` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/default_40_plate_process/replay_1/ElevationProfile.png` |
| `default_40_plate_process` | 1 | `ContactSheet` | n/a | n/a | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/default_40_plate_process/replay_1/ContactSheet.png` |
| `default_40_plate_process_cadence` | 0 | `PlateId` | `b9c86fc53e5e4202` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/default_40_plate_process_cadence/replay_0/PlateId.png` |
| `default_40_plate_process_cadence` | 0 | `CrustType` | `fa9d15f9de85a0b0` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/default_40_plate_process_cadence/replay_0/CrustType.png` |
| `default_40_plate_process_cadence` | 0 | `PlateBoundaries` | `cf4a45d7e3fbdbcd` | 1647345 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/default_40_plate_process_cadence/replay_0/PlateBoundaries.png` |
| `default_40_plate_process_cadence` | 0 | `VelocityField` | `abf019a0bc5bdf86` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/default_40_plate_process_cadence/replay_0/VelocityField.png` |
| `default_40_plate_process_cadence` | 0 | `SubductionRoles` | `1852e69f3c8cf04e` | 1647193 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/default_40_plate_process_cadence/replay_0/SubductionRoles.png` |
| `default_40_plate_process_cadence` | 0 | `Elevation` | `67e3f92ce5fd50af` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/default_40_plate_process_cadence/replay_0/Elevation.png` |
| `default_40_plate_process_cadence` | 0 | `CombinedTectonicSummary` | `6d9865ef09197f28` | 1647354 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/default_40_plate_process_cadence/replay_0/CombinedTectonicSummary.png` |
| `default_40_plate_process_cadence` | 0 | `DistanceToFront` | `e84fcd9e8c3f248b` | 1647230 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/default_40_plate_process_cadence/replay_0/DistanceToFront.png` |
| `default_40_plate_process_cadence` | 0 | `ElevationProfile` | `aae1ca897a1a691b` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/default_40_plate_process_cadence/replay_0/ElevationProfile.png` |
| `default_40_plate_process_cadence` | 0 | `ContactSheet` | n/a | n/a | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/default_40_plate_process_cadence/replay_0/ContactSheet.png` |
| `default_40_plate_process_cadence` | 1 | `PlateId` | `b9c86fc53e5e4202` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/default_40_plate_process_cadence/replay_1/PlateId.png` |
| `default_40_plate_process_cadence` | 1 | `CrustType` | `fa9d15f9de85a0b0` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/default_40_plate_process_cadence/replay_1/CrustType.png` |
| `default_40_plate_process_cadence` | 1 | `PlateBoundaries` | `cf4a45d7e3fbdbcd` | 1647345 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/default_40_plate_process_cadence/replay_1/PlateBoundaries.png` |
| `default_40_plate_process_cadence` | 1 | `VelocityField` | `abf019a0bc5bdf86` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/default_40_plate_process_cadence/replay_1/VelocityField.png` |
| `default_40_plate_process_cadence` | 1 | `SubductionRoles` | `1852e69f3c8cf04e` | 1647193 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/default_40_plate_process_cadence/replay_1/SubductionRoles.png` |
| `default_40_plate_process_cadence` | 1 | `Elevation` | `67e3f92ce5fd50af` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/default_40_plate_process_cadence/replay_1/Elevation.png` |
| `default_40_plate_process_cadence` | 1 | `CombinedTectonicSummary` | `6d9865ef09197f28` | 1647354 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/default_40_plate_process_cadence/replay_1/CombinedTectonicSummary.png` |
| `default_40_plate_process_cadence` | 1 | `DistanceToFront` | `e84fcd9e8c3f248b` | 1647230 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/default_40_plate_process_cadence/replay_1/DistanceToFront.png` |
| `default_40_plate_process_cadence` | 1 | `ElevationProfile` | `aae1ca897a1a691b` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/default_40_plate_process_cadence/replay_1/ElevationProfile.png` |
| `default_40_plate_process_cadence` | 1 | `ContactSheet` | n/a | n/a | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICMapExport/20260503T030833Z/default_40_plate_process_cadence/replay_1/ContactSheet.png` |

## Interpretation

- The export names now follow the Aurous-style diagnostic grammar: `PlateId`, `CrustType`, `PlateBoundaries`, `VelocityField`, `SubductionRoles`, `Elevation`, `CombinedTectonicSummary`, `DistanceToFront`, and `ElevationProfile`.
- `PlateId` is the projected plate-id map with distinct colors per resolved plate. Use it to check whether all plates are represented before reading process overlays.
- `CrustType` is the calm base map: blue ocean, green land, no process overlay.
- `PlateBoundaries` adds thin light edges rasterized from current moved plate-local boundary triangles, drawing only edges whose source endpoints belong to different plates. It no longer uses the global sample boundary mask or draws every boundary-triangle outline. This is the first map to check when the summary feels noisy.
- `VelocityField` adds sparse red plate-motion arrows over the base map.
- `SubductionRoles` is now rasterized directly from current plate-local triangle geometry. It no longer round-trips through `GlobalSampleId`, so the moving role marks should line up with the current moving plate geometry.
- `Elevation` shows the IIIC.2 trench split and IIIC.3 overriding uplift as scalar-field color on the filled continental/oceanic base map.
- `CombinedTectonicSummary` is deliberately restrained: crust + current plate-local boundaries + velocity + subduction roles. Elevation remains separate so uplift heat does not swamp the overview.
- `DistanceToFront` is also rasterized from current plate-local active-boundary triangles. It is diagnostic context, not a source of authority.
- Contact sheet headers include the simulation step and approximate `Myr` timestamp using the CarrierLab `2 Ma` timestep.
- `ElevationProfile` plots uplift delta against distance-to-front and includes the expected thesis distance-transfer curve as a visual shape reference. It is paired with the IIIC.3 numeric oracle; the plot alone is not a gate.

## Cadence Interpretation

- The paper describes oceanic crust generation / plate resampling as periodic every `10-60` time steps depending on observed maximum plate speed; the thesis extraction refines this as `DeltaT = (1-alpha)M + alpha m`, with `alpha = min(1, vm/v0)`, `M=128 Ma`, and `m=32 Ma`.
- With CarrierLab's `2 Ma` timestep, the thesis formula maps to `16-64` steps; the paper's `10-60` statement is treated as the paper-level cadence range, while the actor reports the exact thesis-derived cadence it used for each fixture.
- The actor now reports and, when explicitly enabled, fires cadence from observed plate motion (`vm`) rather than only the configured scalar speed. This matters once slab pull or later process state changes plate motion.
- `default_40_plate_process` intentionally keeps natural resampling off as the rigid-window visual baseline. `default_40_plate_process_cadence` enables natural resampling and must show at least one event by step 40 under the default 40-plate speed. Because IIIC marks are enabled in that fixture, the event uses filtered remeshing rather than the older unfiltered manual-resample shortcut.

## Recommendation

IIIC map export passes. These images are suitable human spatial sanity artifacts for the consolidated IIIC process layer before Phase IIID work begins; they remain read-only evidence maps, not terrain morphology reproduction.
