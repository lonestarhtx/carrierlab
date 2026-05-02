# Phase III Slice IIIB.4 Checkpoint: Oceanic-Under-Continental Polarity Rule

Artifacts root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIB4/20260502T082007Z`

This slice evaluates polarity for existing `SubductionMatrix` plate-pair entries. It is read-only: it does not mark triangles, filter projection candidates, resample material, or advance any IIIC mutation path.

## Gate Summary

| Gate | Result | Evidence |
|---|---|---|
| Mixed oceanic/continental polarity | pass | plate 1 under plate 0, hash `847ef52c79a39bc1` |
| Polarity-swap fixture flips under/over | pass | mixed under 1, swapped under 0, swapped hash `da8ebc8870e9fcdf` |
| Continental-continental emits collision-candidate only | pass | collision 1, subduction polarity 0 |
| Ocean-ocean defers to IIIB.5 | pass | deferred 1, subduction polarity 0 |
| Forced divergence remains empty | pass | matrix pairs 0, decisions 0 |
| Zero-motion remains empty and stable | pass | initial `44bd2ad473ccf5e6`, final `44bd2ad473ccf5e6` |

## Polarity Audits

| Fixture | Replay | Step | Matrix pairs | Decisions | Oceanic-under | Collision | Ocean-ocean deferred | Under | Over | A/B continental fraction | Polarity hash | Convergence hash |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|---|
| mixed_oceanic_under_continental | 0 | 4 | 1 | 1 | 1 | 0 | 0 | 1 | 0 | 1.000 / 0.000 | `847ef52c79a39bc1` | `eae917807ee040f6` |
| mixed_oceanic_under_continental | 1 | 4 | 1 | 1 | 1 | 0 | 0 | 1 | 0 | 1.000 / 0.000 | `847ef52c79a39bc1` | `eae917807ee040f6` |
| mixed_polarity_swap | 0 | 4 | 1 | 1 | 1 | 0 | 0 | 0 | 1 | 0.000 / 1.000 | `da8ebc8870e9fcdf` | `819b1e4fff3d64fc` |
| mixed_polarity_swap | 1 | 4 | 1 | 1 | 1 | 0 | 0 | 0 | 1 | 0.000 / 1.000 | `da8ebc8870e9fcdf` | `819b1e4fff3d64fc` |
| continental_continental_collision_candidate | 0 | 4 | 1 | 1 | 0 | 1 | 0 | -1 | -1 | 1.000 / 1.000 | `345b07f21724121d` | `dcab9c5acb91754e` |
| continental_continental_collision_candidate | 1 | 4 | 1 | 1 | 0 | 1 | 0 | -1 | -1 | 1.000 / 1.000 | `345b07f21724121d` | `dcab9c5acb91754e` |
| ocean_ocean_deferred | 0 | 4 | 1 | 1 | 0 | 0 | 1 | -1 | -1 | 0.000 / 0.000 | `ffb95086e2093e38` | `480c8dc9c3794f2f` |
| ocean_ocean_deferred | 1 | 4 | 1 | 1 | 0 | 0 | 1 | -1 | -1 | 0.000 / 0.000 | `ffb95086e2093e38` | `480c8dc9c3794f2f` |
| forced_divergence_empty | 0 | 1 | 0 | 0 | 0 | 0 | 0 | -1 | -1 | 0.000 / 0.000 | `44bd2ad473ccf5e6` | `301f97fa74365167` |
| forced_divergence_empty | 1 | 1 | 0 | 0 | 0 | 0 | 0 | -1 | -1 | 0.000 / 0.000 | `44bd2ad473ccf5e6` | `301f97fa74365167` |
| zero_motion_empty | 0 | 10 | 0 | 0 | 0 | 0 | 0 | -1 | -1 | 0.000 / 0.000 | `44bd2ad473ccf5e6` | `cbb06e170514c519` |
| zero_motion_empty | 1 | 10 | 0 | 0 | 0 | 0 | 0 | -1 | -1 | 0.000 / 0.000 | `44bd2ad473ccf5e6` | `cbb06e170514c519` |

## Notes

- Dominant material is computed from plate-local vertex continental fraction, not from persistent global ownership.
- `OceanicUnderContinental` records an under/over plate pair but does not mark any triangle as subducting; IIIC owns triangle marking and filter integration.
- Continental-continental entries are logged as collision candidates for IIID. Ocean-ocean entries are deferred to IIIB.5 for age polarity.
- Empty matrix controls produce no polarity decisions, preserving IIIB.3's forced-divergence and zero-motion gates.

## Recommendation

IIIB.4 passes. Pause for user review before IIIB.5 (ocean-ocean age polarity rule).
