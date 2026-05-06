# Phase III Slice IIIB.5 Checkpoint: Ocean-Ocean Age Polarity Rule

Artifacts root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIB5/20260506T205548Z`

This slice completes the read-only polarity decision layer for oceanic-oceanic convergence. It uses plate-local `OceanicAge` as inert crust authority and records older-oceanic-under-younger decisions without marking triangles, filtering projection candidates, resampling material, or mutating process state.

## Gate Summary

| Gate | Result | Evidence |
|---|---|---|
| Older plate 0 subducts under younger plate 1 | pass | ages 120.0 / 20.0 Ma, under 0, hash `4108cd9803d74f8e` |
| Reversing ages reverses polarity | pass | older0 under 0, older1 under 1, reversed hash `24faf969899a12ac` |
| Equal-age ocean-ocean remains deferred | pass | deferred 1, age-polarity 0 |
| Mixed-material IIIB.4 regression unchanged | pass | oceanic-under 1, age-polarity 0 |
| Continental collision regression unchanged | pass | collision 1, subduction polarity 0 |
| Forced-divergence local evidence applies age polarity deterministically | pass | matrix pairs 1, decisions 1, local under 0 over 1 |
| Zero-motion remains empty and stable | pass | initial `44bd2ad473ccf5e6`, final `44bd2ad473ccf5e6` |

## Polarity Audits

| Fixture | Replay | Step | Matrix pairs | Decisions | Oceanic-under | Age-polarity | Collision | Ocean-ocean deferred | Under | Over | A/B oceanic age Ma | Polarity hash | Convergence hash |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|---|
| ocean_ocean_plate0_older | 0 | 4 | 1 | 1 | 0 | 1 | 0 | 0 | 0 | 1 | 120.000 / 20.000 | `4108cd9803d74f8e` | `60d9218b8a64b2d5` |
| ocean_ocean_plate0_older | 1 | 4 | 1 | 1 | 0 | 1 | 0 | 0 | 0 | 1 | 120.000 / 20.000 | `4108cd9803d74f8e` | `60d9218b8a64b2d5` |
| ocean_ocean_plate1_older | 0 | 4 | 1 | 1 | 0 | 1 | 0 | 0 | 1 | 0 | 20.000 / 120.000 | `24faf969899a12ac` | `b1042f397f5c866c` |
| ocean_ocean_plate1_older | 1 | 4 | 1 | 1 | 0 | 1 | 0 | 0 | 1 | 0 | 20.000 / 120.000 | `24faf969899a12ac` | `b1042f397f5c866c` |
| ocean_ocean_equal_age_deferred | 0 | 4 | 1 | 1 | 0 | 0 | 0 | 1 | -1 | -1 | 64.000 / 64.000 | `ad9bc3e623d392d8` | `d6155da819731d47` |
| ocean_ocean_equal_age_deferred | 1 | 4 | 1 | 1 | 0 | 0 | 0 | 1 | -1 | -1 | 64.000 / 64.000 | `ad9bc3e623d392d8` | `d6155da819731d47` |
| mixed_material_regression | 0 | 4 | 1 | 1 | 1 | 0 | 0 | 0 | 1 | 0 | 20.000 / 120.000 | `385d134faf6a32f9` | `0d7fd1558364ce5d` |
| mixed_material_regression | 1 | 4 | 1 | 1 | 1 | 0 | 0 | 0 | 1 | 0 | 20.000 / 120.000 | `385d134faf6a32f9` | `0d7fd1558364ce5d` |
| continental_collision_regression | 0 | 4 | 1 | 1 | 0 | 0 | 1 | 0 | -1 | -1 | 120.000 / 20.000 | `edb153e7f34d4ebd` | `894fa665518dda0a` |
| continental_collision_regression | 1 | 4 | 1 | 1 | 0 | 0 | 1 | 0 | -1 | -1 | 120.000 / 20.000 | `edb153e7f34d4ebd` | `894fa665518dda0a` |
| forced_divergence_empty | 0 | 1 | 1 | 1 | 0 | 1 | 0 | 0 | 0 | 1 | 120.000 / 20.000 | `4108cd9803d74f8e` | `5f5cfc46b44075ce` |
| forced_divergence_empty | 1 | 1 | 1 | 1 | 0 | 1 | 0 | 0 | 0 | 1 | 120.000 / 20.000 | `4108cd9803d74f8e` | `5f5cfc46b44075ce` |
| zero_motion_empty | 0 | 10 | 0 | 0 | 0 | 0 | 0 | 0 | -1 | -1 | 0.000 / 0.000 | `44bd2ad473ccf5e6` | `33f1e105a5121513` |
| zero_motion_empty | 1 | 10 | 0 | 0 | 0 | 0 | 0 | 0 | -1 | -1 | 0.000 / 0.000 | `44bd2ad473ccf5e6` | `33f1e105a5121513` |

## Notes

- Oceanic age is averaged from plate-local vertices using area weights. Global samples are updated only by the explicit test seeding helper so fixtures survive projection/replay checks.
- Equal oceanic ages still defer, preserving the IIIB.4 no-invented-policy discipline when age evidence cannot distinguish polarity.
- IIIB.3 currently admits local convergent backside evidence in the forced-divergence fixture on the closed sphere; IIIB.5 applies age polarity to that inherited local matrix evidence rather than using it as an empty-matrix negative.
- Zero-motion remains the empty-matrix negative control and produces no polarity decisions.
- The age rule is a decision record only. IIIB.6 may propagate from active decisions, but this slice does not label neighbors, filter resampling, or mutate crust.

## Recommendation

IIIB.5 passes. Pause for user review before IIIB.6 (neighbor propagation).
