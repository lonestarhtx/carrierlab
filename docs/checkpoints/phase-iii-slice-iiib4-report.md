# Phase III Slice IIIB.4 Checkpoint: Oceanic-Under-Continental Polarity Rule

Artifacts root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIB4/20260506T205541Z`

This slice evaluates polarity for existing `SubductionMatrix` plate-pair entries. It is read-only: it does not mark triangles, filter projection candidates, resample material, or advance any IIIC mutation path.

## Gate Summary

| Gate | Result | Evidence |
|---|---|---|
| Mixed oceanic/continental polarity | pass | plate 1 under plate 0, hash `a8139942bc021af9` |
| Polarity-swap fixture flips under/over | pass | mixed under 1, swapped under 0, swapped hash `4dcaddcd944136a7` |
| Continental-continental emits collision-candidate only | pass | collision 1, subduction polarity 0 |
| Ocean-ocean defers to IIIB.5 | pass | deferred 1, subduction polarity 0 |
| Forced-divergence local evidence classifies deterministically | pass | matrix pairs 1, decisions 1, local under 1 over 0 |
| Zero-motion remains empty and stable | pass | initial `44bd2ad473ccf5e6`, final `44bd2ad473ccf5e6` |

## Polarity Audits

| Fixture | Replay | Step | Matrix pairs | Decisions | Oceanic-under | Collision | Ocean-ocean deferred | Under | Over | A/B continental fraction | Polarity hash | Convergence hash |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|---|
| mixed_oceanic_under_continental | 0 | 4 | 1 | 1 | 1 | 0 | 0 | 1 | 0 | 1.000 / 0.000 | `a8139942bc021af9` | `825d17a81802865d` |
| mixed_oceanic_under_continental | 1 | 4 | 1 | 1 | 1 | 0 | 0 | 1 | 0 | 1.000 / 0.000 | `a8139942bc021af9` | `825d17a81802865d` |
| mixed_polarity_swap | 0 | 4 | 1 | 1 | 1 | 0 | 0 | 0 | 1 | 0.000 / 1.000 | `4dcaddcd944136a7` | `8af0022b81d9ebac` |
| mixed_polarity_swap | 1 | 4 | 1 | 1 | 1 | 0 | 0 | 0 | 1 | 0.000 / 1.000 | `4dcaddcd944136a7` | `8af0022b81d9ebac` |
| continental_continental_collision_candidate | 0 | 4 | 1 | 1 | 0 | 1 | 0 | -1 | -1 | 1.000 / 1.000 | `a6163e92c658a6bd` | `c591679dd39cb20a` |
| continental_continental_collision_candidate | 1 | 4 | 1 | 1 | 0 | 1 | 0 | -1 | -1 | 1.000 / 1.000 | `a6163e92c658a6bd` | `c591679dd39cb20a` |
| ocean_ocean_deferred | 0 | 4 | 1 | 1 | 0 | 0 | 1 | -1 | -1 | 0.000 / 0.000 | `743b7c1af5b692d8` | `96a9b3bad5b01d47` |
| ocean_ocean_deferred | 1 | 4 | 1 | 1 | 0 | 0 | 1 | -1 | -1 | 0.000 / 0.000 | `743b7c1af5b692d8` | `96a9b3bad5b01d47` |
| forced_divergence_empty | 0 | 1 | 1 | 1 | 1 | 0 | 0 | 1 | 0 | 1.000 / 0.000 | `a8139942bc021af9` | `512545ebf1ffabad` |
| forced_divergence_empty | 1 | 1 | 1 | 1 | 1 | 0 | 0 | 1 | 0 | 1.000 / 0.000 | `a8139942bc021af9` | `512545ebf1ffabad` |
| zero_motion_empty | 0 | 10 | 0 | 0 | 0 | 0 | 0 | -1 | -1 | 0.000 / 0.000 | `44bd2ad473ccf5e6` | `33f1e105a5121513` |
| zero_motion_empty | 1 | 10 | 0 | 0 | 0 | 0 | 0 | -1 | -1 | 0.000 / 0.000 | `44bd2ad473ccf5e6` | `33f1e105a5121513` |

## Notes

- Dominant material is computed from plate-local vertex continental fraction, not from persistent global ownership.
- `OceanicUnderContinental` records an under/over plate pair but does not mark any triangle as subducting; IIIC owns triangle marking and filter integration.
- Continental-continental entries are logged as collision candidates for IIID. Ocean-ocean entries are deferred to IIIB.5 for age polarity.
- IIIB.3 currently admits local convergent backside evidence in the forced-divergence fixture on the closed sphere; IIIB.4 classifies that inherited matrix evidence rather than re-gating pair-wide divergence.
- Zero-motion remains the empty-matrix negative control and produces no polarity decisions.

## Recommendation

IIIB.4 passes. Pause for user review before IIIB.5 (ocean-ocean age polarity rule).
