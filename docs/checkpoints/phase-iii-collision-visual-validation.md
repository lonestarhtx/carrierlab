# Phase III Collision Visual Validation Checkpoint

Status: read-only map export around the accepted IIID forced-collision actor path.

## Scope

This checkpoint exports filled Mollweide-style PNG artifacts before and after the existing IIID collision path mutates plate-local topology and applies IIID.7 uplift. The export path itself is read-only; the post-collision fixture intentionally invokes the already accepted IIID actor mutation so the resulting maps are visually inspectable. This checkpoint does not add new simulation behavior, resampling behavior, authority fallback patterns, projection-side correction, terrain displacement, or paper-remesh claims.

Fixtures:

- `forced_collision_ready`: Two-plate forced-convergence fixture at the collision threshold with the same destination-patch scaffold used by the post-collision fixture, before applying the IIID mutation. (`10k / 2 plates / seed 42 / 8 steps / forced_convergence motion / default material / natural resampling off / IIIC process on / slab pull off / IIID visual on / IIID apply off / expected informational / centroid policy`).
- `forced_collision_after`: Same forced-convergence fixture after applying the existing IIID detach/suture/uplift event. This is visual validation only; the mutation path is the accepted IIID actor path. (`10k / 2 plates / seed 42 / 8 steps / forced_convergence motion / default material / natural resampling off / IIIC process on / slab pull off / IIID visual on / IIID apply on / expected collision_signals_required / centroid policy`).
- `forced_collision_front_ready`: Two-plate forced-convergence fixture with all accepted destination-side front triangles made continental before applying IIID mutation. This asks whether the current collision model can present a ridge-scale candidate front. (`10k / 2 plates / seed 42 / 8 steps / forced_convergence motion / default material / natural resampling off / IIIC process on / slab pull off / IIID visual on / IIID apply off / expected informational / centroid policy`).
- `forced_collision_front_after`: Same broad-front fixture after applying the existing IIID detach/suture/uplift event. This is a visual falsification fixture for the mountain-range question, not a new paper-faithfulness claim. (`10k / 2 plates / seed 42 / 8 steps / forced_convergence motion / default material / natural resampling off / IIIC process on / slab pull off / IIID visual on / IIID apply on / expected collision_signals_required / centroid policy`).

## Gate Summary

| Gate | Result | Evidence |
|---|---:|---|
| map exports written | pass | output root `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z` |
| `forced_collision_ready` export read-only | pass | replay 0 `unchanged hashes`, replay 1 `unchanged hashes` |
| `forced_collision_ready` same-seed map hashes | pass | replay hashes byte-identical per layer |
| `forced_collision_ready` expected spatial signal | pass | expected `informational` |
| `forced_collision_ready` cadence behavior | pass | natural resampling `off`, events 0/0, cadence 16 steps / 32.000 Ma, observed vmax 120.000000 mm/yr |
| `forced_collision_after` export read-only | pass | replay 0 `unchanged hashes`, replay 1 `unchanged hashes` |
| `forced_collision_after` same-seed map hashes | pass | replay hashes byte-identical per layer |
| `forced_collision_after` expected spatial signal | pass | expected `collision_signals_required` |
| `forced_collision_after` cadence behavior | pass | natural resampling `off`, events 1/1, cadence 16 steps / 32.000 Ma, observed vmax 120.000000 mm/yr |
| `forced_collision_front_ready` export read-only | pass | replay 0 `unchanged hashes`, replay 1 `unchanged hashes` |
| `forced_collision_front_ready` same-seed map hashes | pass | replay hashes byte-identical per layer |
| `forced_collision_front_ready` expected spatial signal | pass | expected `informational` |
| `forced_collision_front_ready` cadence behavior | pass | natural resampling `off`, events 0/0, cadence 16 steps / 32.000 Ma, observed vmax 120.000000 mm/yr |
| `forced_collision_front_after` export read-only | pass | replay 0 `unchanged hashes`, replay 1 `unchanged hashes` |
| `forced_collision_front_after` same-seed map hashes | pass | replay hashes byte-identical per layer |
| `forced_collision_front_after` expected spatial signal | pass | expected `collision_signals_required` |
| `forced_collision_front_after` cadence behavior | pass | natural resampling `off`, events 1/1, cadence 16 steps / 32.000 Ma, observed vmax 120.000000 mm/yr |

## Cadence State

| Scenario | Replay | Step | Events | Next resample | Cadence steps | DeltaT Ma | Observed max speed mm/yr | Reset serial |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `forced_collision_ready` | 0 | 2 | 0 | 16 | 16 | 32.000000 | 120.000000000 | 0 |
| `forced_collision_ready` | 1 | 2 | 0 | 16 | 16 | 32.000000 | 120.000000000 | 0 |
| `forced_collision_after` | 0 | 2 | 1 | 16 | 16 | 32.000000 | 120.000000000 | 1 |
| `forced_collision_after` | 1 | 2 | 1 | 16 | 16 | 32.000000 | 120.000000000 | 1 |
| `forced_collision_front_ready` | 0 | 2 | 0 | 16 | 16 | 32.000000 | 120.000000000 | 0 |
| `forced_collision_front_ready` | 1 | 2 | 0 | 16 | 16 | 32.000000 | 120.000000000 | 0 |
| `forced_collision_front_after` | 0 | 2 | 1 | 16 | 16 | 32.000000 | 120.000000000 | 1 |
| `forced_collision_front_after` | 1 | 2 | 1 | 16 | 16 | 32.000000 | 120.000000000 | 1 |

## Tracking State Used For Maps

| Scenario | Replay | Step | Active triangles | Distance records | Matrix pairs | Polarity decisions | Subduction polarity | Propagation seeds | Propagation added | Convergence hash |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| `forced_collision_ready` | 0 | 2 | 362 | 362 | 1 | 1 | 0 | 0 | 0 | `ad57da689c811399` |
| `forced_collision_ready` | 1 | 2 | 362 | 362 | 1 | 1 | 0 | 0 | 0 | `ad57da689c811399` |
| `forced_collision_after` | 0 | 2 | 366 | 366 | 0 | 0 | 0 | 0 | 0 | `ff38089d57d1e141` |
| `forced_collision_after` | 1 | 2 | 366 | 366 | 0 | 0 | 0 | 0 | 0 | `ff38089d57d1e141` |
| `forced_collision_front_ready` | 0 | 2 | 362 | 362 | 1 | 1 | 0 | 0 | 0 | `ad57da689c811399` |
| `forced_collision_front_ready` | 1 | 2 | 362 | 362 | 1 | 1 | 0 | 0 | 0 | `ad57da689c811399` |
| `forced_collision_front_after` | 0 | 2 | 650 | 650 | 0 | 0 | 0 | 0 | 0 | `11d1e719cc95c9cb` |
| `forced_collision_front_after` | 1 | 2 | 650 | 650 | 0 | 0 | 0 | 0 | 0 | `11d1e719cc95c9cb` |

## IIIC State Used For Maps

| Scenario | Replay | Process layer | Slab pull | Marks | Trench records | Uplift records | Ledger records | Actual delta km | Ledger residual km | Mark hash | Elevation hash | Uplift hash | Ledger hash |
|---|---:|---|---|---:|---:|---:|---:|---:|---:|---|---|---|---|
| `forced_collision_ready` | 0 | on | off | 0 | 0 | 26709 | 26709 | 6743.534573952576 | -6.366462912410e-12 | `e5ad224d9efbf9dc` | `10780f9774dc5c95` | `6aeb3c6adb45f65b` | `1e660046c6deeebf` |
| `forced_collision_ready` | 1 | on | off | 0 | 0 | 26709 | 26709 | 6743.534573952576 | -6.366462912410e-12 | `e5ad224d9efbf9dc` | `10780f9774dc5c95` | `6aeb3c6adb45f65b` | `1e660046c6deeebf` |
| `forced_collision_after` | 0 | on | off | 0 | 0 | 26709 | 26709 | 6743.534573952576 | -6.366462912410e-12 | `7cfe5355f049b7c2` | `60c3463e37187998` | `6aeb3c6adb45f65b` | `1e660046c6deeebf` |
| `forced_collision_after` | 1 | on | off | 0 | 0 | 26709 | 26709 | 6743.534573952576 | -6.366462912410e-12 | `7cfe5355f049b7c2` | `60c3463e37187998` | `6aeb3c6adb45f65b` | `1e660046c6deeebf` |
| `forced_collision_front_ready` | 0 | on | off | 0 | 0 | 26709 | 26709 | 6743.534573952576 | -6.366462912410e-12 | `e5ad224d9efbf9dc` | `10780f9774dc5c95` | `6aeb3c6adb45f65b` | `1e660046c6deeebf` |
| `forced_collision_front_ready` | 1 | on | off | 0 | 0 | 26709 | 26709 | 6743.534573952576 | -6.366462912410e-12 | `e5ad224d9efbf9dc` | `10780f9774dc5c95` | `6aeb3c6adb45f65b` | `1e660046c6deeebf` |
| `forced_collision_front_after` | 0 | on | off | 0 | 0 | 26709 | 26709 | 6743.534573952576 | -6.366462912410e-12 | `7cfe5355f049b7c2` | `0b4ef5fb3a29ccfe` | `6aeb3c6adb45f65b` | `1e660046c6deeebf` |
| `forced_collision_front_after` | 1 | on | off | 0 | 0 | 26709 | 26709 | 6743.534573952576 | -6.366462912410e-12 | `7cfe5355f049b7c2` | `0b4ef5fb3a29ccfe` | `6aeb3c6adb45f65b` | `1e660046c6deeebf` |

## IIID Collision State Used For Maps

| Scenario | Replay | Collision applied | Collision step | Accepted groups | Mutations | Deferred plans | Reset | Removed tris | Added tris | Uplift records | Total uplift km | Policy multi-hits | Patch seed/count | Topology hash | Uplift hash |
|---|---:|---|---:|---:|---:|---:|---|---:|---:|---:|---:|---:|---|---|---|
| `forced_collision_ready` | 0 | no | 2 | 1 | 0 | 0 | 0->0 | 0 | 0 | 0 | 0.000000000000 | 0 | 7007/1 | `` | `` |
| `forced_collision_ready` | 1 | no | 2 | 1 | 0 | 0 | 0->0 | 0 | 0 | 0 | 0.000000000000 | 0 | 7007/1 | `` | `` |
| `forced_collision_after` | 0 | yes | 2 | 1 | 1 | 0 | 0->1 | 4 | 4 | 3 | 7.957005760028 | 0 | 7007/1 | `eaf45a2e2cf791b6` | `adb3a7c072104afc` |
| `forced_collision_after` | 1 | yes | 2 | 1 | 1 | 0 | 0->1 | 4 | 4 | 3 | 7.957005760028 | 0 | 7007/1 | `eaf45a2e2cf791b6` | `adb3a7c072104afc` |
| `forced_collision_front_ready` | 0 | no | 2 | 1 | 0 | 0 | 0->0 | 0 | 0 | 0 | 0.000000000000 | 0 | 7007/263 | `` | `` |
| `forced_collision_front_ready` | 1 | no | 2 | 1 | 0 | 0 | 0->0 | 0 | 0 | 0 | 0.000000000000 | 0 | 7007/263 | `` | `` |
| `forced_collision_front_after` | 0 | yes | 2 | 1 | 1 | 0 | 0->1 | 500 | 500 | 757 | 204711.127681681362 | 0 | 7007/263 | `1e829ef4e2a39ebf` | `e1aec6f33b2d41c7` |
| `forced_collision_front_after` | 1 | yes | 2 | 1 | 1 | 0 | 0->1 | 500 | 500 | 757 | 204711.127681681362 | 0 | 7007/263 | `1e829ef4e2a39ebf` | `e1aec6f33b2d41c7` |

This collision visual checkpoint gates only that the accepted forced fixture produced deterministic before/after maps and that no lab multi-hit policy resolved the collision evidence. The `forced_collision_ready` snapshot uses the same small destination-patch scaffold as `forced_collision_after`; broad blue/green differences between this visual fixture and natural world maps are fixture scaffolding, not a claimed geological output. It is not a Slice 5.5, Stage 1.5, or thesis-remesh success claim.

## Exported Maps

| Scenario | Replay | Layer | Hash | Non-background pixels | Path |
|---|---:|---|---|---:|---|
| `forced_collision_ready` | 0 | `PlateId` | `3fc91e14d0178348` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_ready/replay_0/PlateId.png` |
| `forced_collision_ready` | 0 | `CrustType` | `f356e343ea924f2e` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_ready/replay_0/CrustType.png` |
| `forced_collision_ready` | 0 | `PlateBoundaries` | `964b173f254ce791` | 1647143 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_ready/replay_0/PlateBoundaries.png` |
| `forced_collision_ready` | 0 | `VelocityField` | `647f9dec432d035e` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_ready/replay_0/VelocityField.png` |
| `forced_collision_ready` | 0 | `SubductionRoles` | `d6949568d5967b89` | 1647143 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_ready/replay_0/SubductionRoles.png` |
| `forced_collision_ready` | 0 | `Elevation` | `ead4c7cf9a88f695` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_ready/replay_0/Elevation.png` |
| `forced_collision_ready` | 0 | `CombinedTectonicSummary` | `ac38501ec1594344` | 1647143 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_ready/replay_0/CombinedTectonicSummary.png` |
| `forced_collision_ready` | 0 | `DistanceToFront` | `b551b59cd48bf8e8` | 1647143 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_ready/replay_0/DistanceToFront.png` |
| `forced_collision_ready` | 0 | `ElevationProfile` | `041955de1747fc4a` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_ready/replay_0/ElevationProfile.png` |
| `forced_collision_ready` | 0 | `ContactSheet` | n/a | n/a | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_ready/replay_0/ContactSheet.png` |
| `forced_collision_ready` | 1 | `PlateId` | `3fc91e14d0178348` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_ready/replay_1/PlateId.png` |
| `forced_collision_ready` | 1 | `CrustType` | `f356e343ea924f2e` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_ready/replay_1/CrustType.png` |
| `forced_collision_ready` | 1 | `PlateBoundaries` | `964b173f254ce791` | 1647143 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_ready/replay_1/PlateBoundaries.png` |
| `forced_collision_ready` | 1 | `VelocityField` | `647f9dec432d035e` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_ready/replay_1/VelocityField.png` |
| `forced_collision_ready` | 1 | `SubductionRoles` | `d6949568d5967b89` | 1647143 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_ready/replay_1/SubductionRoles.png` |
| `forced_collision_ready` | 1 | `Elevation` | `ead4c7cf9a88f695` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_ready/replay_1/Elevation.png` |
| `forced_collision_ready` | 1 | `CombinedTectonicSummary` | `ac38501ec1594344` | 1647143 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_ready/replay_1/CombinedTectonicSummary.png` |
| `forced_collision_ready` | 1 | `DistanceToFront` | `b551b59cd48bf8e8` | 1647143 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_ready/replay_1/DistanceToFront.png` |
| `forced_collision_ready` | 1 | `ElevationProfile` | `041955de1747fc4a` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_ready/replay_1/ElevationProfile.png` |
| `forced_collision_ready` | 1 | `ContactSheet` | n/a | n/a | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_ready/replay_1/ContactSheet.png` |
| `forced_collision_after` | 0 | `PlateId` | `82e918b71dcbba28` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_after/replay_0/PlateId.png` |
| `forced_collision_after` | 0 | `CrustType` | `f356e343ea924f2e` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_after/replay_0/CrustType.png` |
| `forced_collision_after` | 0 | `PlateBoundaries` | `964b173f254ce791` | 1647143 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_after/replay_0/PlateBoundaries.png` |
| `forced_collision_after` | 0 | `VelocityField` | `647f9dec432d035e` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_after/replay_0/VelocityField.png` |
| `forced_collision_after` | 0 | `SubductionRoles` | `a86a0f1b2edadfb8` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_after/replay_0/SubductionRoles.png` |
| `forced_collision_after` | 0 | `Elevation` | `2611dac1e3cbe2f9` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_after/replay_0/Elevation.png` |
| `forced_collision_after` | 0 | `CombinedTectonicSummary` | `b218f3ae10031fb1` | 1647143 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_after/replay_0/CombinedTectonicSummary.png` |
| `forced_collision_after` | 0 | `DistanceToFront` | `13524b9e66a33ccf` | 1647143 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_after/replay_0/DistanceToFront.png` |
| `forced_collision_after` | 0 | `CollisionOverlay` | `ed22476ea15476b5` | 1647143 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_after/replay_0/CollisionOverlay.png` |
| `forced_collision_after` | 0 | `CollisionDelta` | `5dec951b5c32c837` | 2097152 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_after/replay_0/CollisionDelta.png` |
| `forced_collision_after` | 0 | `CollisionZoom` | `2911d566acbe7f57` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_after/replay_0/CollisionZoom.png` |
| `forced_collision_after` | 0 | `ElevationProfile` | `07b4914ea37573a8` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_after/replay_0/ElevationProfile.png` |
| `forced_collision_after` | 0 | `ContactSheet` | n/a | n/a | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_after/replay_0/ContactSheet.png` |
| `forced_collision_after` | 1 | `PlateId` | `82e918b71dcbba28` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_after/replay_1/PlateId.png` |
| `forced_collision_after` | 1 | `CrustType` | `f356e343ea924f2e` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_after/replay_1/CrustType.png` |
| `forced_collision_after` | 1 | `PlateBoundaries` | `964b173f254ce791` | 1647143 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_after/replay_1/PlateBoundaries.png` |
| `forced_collision_after` | 1 | `VelocityField` | `647f9dec432d035e` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_after/replay_1/VelocityField.png` |
| `forced_collision_after` | 1 | `SubductionRoles` | `a86a0f1b2edadfb8` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_after/replay_1/SubductionRoles.png` |
| `forced_collision_after` | 1 | `Elevation` | `2611dac1e3cbe2f9` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_after/replay_1/Elevation.png` |
| `forced_collision_after` | 1 | `CombinedTectonicSummary` | `b218f3ae10031fb1` | 1647143 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_after/replay_1/CombinedTectonicSummary.png` |
| `forced_collision_after` | 1 | `DistanceToFront` | `13524b9e66a33ccf` | 1647143 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_after/replay_1/DistanceToFront.png` |
| `forced_collision_after` | 1 | `CollisionOverlay` | `ed22476ea15476b5` | 1647143 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_after/replay_1/CollisionOverlay.png` |
| `forced_collision_after` | 1 | `CollisionDelta` | `5dec951b5c32c837` | 2097152 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_after/replay_1/CollisionDelta.png` |
| `forced_collision_after` | 1 | `CollisionZoom` | `2911d566acbe7f57` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_after/replay_1/CollisionZoom.png` |
| `forced_collision_after` | 1 | `ElevationProfile` | `07b4914ea37573a8` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_after/replay_1/ElevationProfile.png` |
| `forced_collision_after` | 1 | `ContactSheet` | n/a | n/a | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_after/replay_1/ContactSheet.png` |
| `forced_collision_front_ready` | 0 | `PlateId` | `3fc91e14d0178348` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_ready/replay_0/PlateId.png` |
| `forced_collision_front_ready` | 0 | `CrustType` | `663f1b2bea2c1f9e` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_ready/replay_0/CrustType.png` |
| `forced_collision_front_ready` | 0 | `PlateBoundaries` | `057c09f61f366c59` | 1647143 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_ready/replay_0/PlateBoundaries.png` |
| `forced_collision_front_ready` | 0 | `VelocityField` | `d00f91c7167316b6` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_ready/replay_0/VelocityField.png` |
| `forced_collision_front_ready` | 0 | `SubductionRoles` | `6eb468c58a34d7b8` | 1647143 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_ready/replay_0/SubductionRoles.png` |
| `forced_collision_front_ready` | 0 | `Elevation` | `ba0aa2f763a75bb3` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_ready/replay_0/Elevation.png` |
| `forced_collision_front_ready` | 0 | `CombinedTectonicSummary` | `ecf71b239317d27c` | 1647143 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_ready/replay_0/CombinedTectonicSummary.png` |
| `forced_collision_front_ready` | 0 | `DistanceToFront` | `111c082df8b52de2` | 1647143 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_ready/replay_0/DistanceToFront.png` |
| `forced_collision_front_ready` | 0 | `ElevationProfile` | `041955de1747fc4a` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_ready/replay_0/ElevationProfile.png` |
| `forced_collision_front_ready` | 0 | `ContactSheet` | n/a | n/a | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_ready/replay_0/ContactSheet.png` |
| `forced_collision_front_ready` | 1 | `PlateId` | `3fc91e14d0178348` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_ready/replay_1/PlateId.png` |
| `forced_collision_front_ready` | 1 | `CrustType` | `663f1b2bea2c1f9e` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_ready/replay_1/CrustType.png` |
| `forced_collision_front_ready` | 1 | `PlateBoundaries` | `057c09f61f366c59` | 1647143 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_ready/replay_1/PlateBoundaries.png` |
| `forced_collision_front_ready` | 1 | `VelocityField` | `d00f91c7167316b6` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_ready/replay_1/VelocityField.png` |
| `forced_collision_front_ready` | 1 | `SubductionRoles` | `6eb468c58a34d7b8` | 1647143 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_ready/replay_1/SubductionRoles.png` |
| `forced_collision_front_ready` | 1 | `Elevation` | `ba0aa2f763a75bb3` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_ready/replay_1/Elevation.png` |
| `forced_collision_front_ready` | 1 | `CombinedTectonicSummary` | `ecf71b239317d27c` | 1647143 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_ready/replay_1/CombinedTectonicSummary.png` |
| `forced_collision_front_ready` | 1 | `DistanceToFront` | `111c082df8b52de2` | 1647143 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_ready/replay_1/DistanceToFront.png` |
| `forced_collision_front_ready` | 1 | `ElevationProfile` | `041955de1747fc4a` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_ready/replay_1/ElevationProfile.png` |
| `forced_collision_front_ready` | 1 | `ContactSheet` | n/a | n/a | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_ready/replay_1/ContactSheet.png` |
| `forced_collision_front_after` | 0 | `PlateId` | `e11c818a39ff0a28` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_after/replay_0/PlateId.png` |
| `forced_collision_front_after` | 0 | `CrustType` | `be0a5b1e4009e6af` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_after/replay_0/CrustType.png` |
| `forced_collision_front_after` | 0 | `PlateBoundaries` | `97d7d9263b93064d` | 1647143 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_after/replay_0/PlateBoundaries.png` |
| `forced_collision_front_after` | 0 | `VelocityField` | `e292cf319c0f3217` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_after/replay_0/VelocityField.png` |
| `forced_collision_front_after` | 0 | `SubductionRoles` | `be1395d60d9a3c36` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_after/replay_0/SubductionRoles.png` |
| `forced_collision_front_after` | 0 | `Elevation` | `e1a627870a7431af` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_after/replay_0/Elevation.png` |
| `forced_collision_front_after` | 0 | `CombinedTectonicSummary` | `b715ca22ca7e75e5` | 1647143 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_after/replay_0/CombinedTectonicSummary.png` |
| `forced_collision_front_after` | 0 | `DistanceToFront` | `fc6df8d1cc5058fc` | 1647148 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_after/replay_0/DistanceToFront.png` |
| `forced_collision_front_after` | 0 | `CollisionOverlay` | `4ea3387c45167b4b` | 1653234 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_after/replay_0/CollisionOverlay.png` |
| `forced_collision_front_after` | 0 | `CollisionDelta` | `be8302b0bf40e688` | 2097152 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_after/replay_0/CollisionDelta.png` |
| `forced_collision_front_after` | 0 | `CollisionZoom` | `c79ffed876662e31` | 420010 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_after/replay_0/CollisionZoom.png` |
| `forced_collision_front_after` | 0 | `ElevationProfile` | `d3725559114a3092` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_after/replay_0/ElevationProfile.png` |
| `forced_collision_front_after` | 0 | `ContactSheet` | n/a | n/a | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_after/replay_0/ContactSheet.png` |
| `forced_collision_front_after` | 1 | `PlateId` | `e11c818a39ff0a28` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_after/replay_1/PlateId.png` |
| `forced_collision_front_after` | 1 | `CrustType` | `be0a5b1e4009e6af` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_after/replay_1/CrustType.png` |
| `forced_collision_front_after` | 1 | `PlateBoundaries` | `97d7d9263b93064d` | 1647143 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_after/replay_1/PlateBoundaries.png` |
| `forced_collision_front_after` | 1 | `VelocityField` | `e292cf319c0f3217` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_after/replay_1/VelocityField.png` |
| `forced_collision_front_after` | 1 | `SubductionRoles` | `be1395d60d9a3c36` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_after/replay_1/SubductionRoles.png` |
| `forced_collision_front_after` | 1 | `Elevation` | `e1a627870a7431af` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_after/replay_1/Elevation.png` |
| `forced_collision_front_after` | 1 | `CombinedTectonicSummary` | `b715ca22ca7e75e5` | 1647143 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_after/replay_1/CombinedTectonicSummary.png` |
| `forced_collision_front_after` | 1 | `DistanceToFront` | `fc6df8d1cc5058fc` | 1647148 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_after/replay_1/DistanceToFront.png` |
| `forced_collision_front_after` | 1 | `CollisionOverlay` | `4ea3387c45167b4b` | 1653234 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_after/replay_1/CollisionOverlay.png` |
| `forced_collision_front_after` | 1 | `CollisionDelta` | `be8302b0bf40e688` | 2097152 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_after/replay_1/CollisionDelta.png` |
| `forced_collision_front_after` | 1 | `CollisionZoom` | `c79ffed876662e31` | 420010 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_after/replay_1/CollisionZoom.png` |
| `forced_collision_front_after` | 1 | `ElevationProfile` | `d3725559114a3092` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_after/replay_1/ElevationProfile.png` |
| `forced_collision_front_after` | 1 | `ContactSheet` | n/a | n/a | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/CollisionVisualValidation/20260506T203758Z/forced_collision_front_after/replay_1/ContactSheet.png` |

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
- `CollisionOverlay`, `CollisionDelta`, and `CollisionZoom` are event-focused diagnostics derived from the existing IIID.7 uplift audit records. They deliberately amplify the small forced fixture so the transferred/uplifted region is visible to a human reviewer; they do not add a new simulation signal.
- Contact sheet headers include the simulation step and approximate `Myr` timestamp using the CarrierLab `2 Ma` timestep.
- `ElevationProfile` plots IIID.7 collision uplift delta against distance-to-terrane for the post-collision fixture. The profile is a visual sanity plot; the IIID.7 commandlet remains the formula gate.

## Recommendation

Collision visual validation passes. Use the before/after contact sheets as human-spatial evidence that the accepted IIID actor path visibly mutates plate-local topology and applies uplift. This is still visual evidence only, not a replacement for commandlet gates or a thesis-remesh claim.
