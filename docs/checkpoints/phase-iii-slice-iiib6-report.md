# Phase III Slice IIIB.6 Checkpoint: Neighbor Propagation

Artifacts root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIB6/20260502T085624Z`

This slice expands read-only convergence tracking from a subducting active triangle to its plate-local edge-neighbor triangles. Propagation only fires for matrix evidence with a subduction polarity decision (`OceanicUnderContinental` or `OlderOceanicUnderYoungerOceanic`), and each new neighbor receives a distance-to-front value bounded by `r_s = 1800 km`. It does not mark triangles, filter projection candidates, resample, or mutate crust material.

## Gate Summary

| Gate | Result | Evidence |
|---|---|---|
| Single-triangle seed grows by expected neighbors | pass | seed 135, candidates 1, active 2 -> 2, added 1 |
| Active list remains bounded | pass | active 556 / local 19996, max distance 1799.844699 km, rejected 142 |
| Equal-age ocean-ocean defers and does not propagate | pass | active 1 -> 1, added 0 |
| Forced divergence has no propagation | pass | seed hits 0, added 0 |
| Zero-motion no-op remains hash-stable | pass | `33f1e105a5121513` -> `33f1e105a5121513` |
| Phase II hashes replay unchanged | pass | projection `e23186b0f3edb278`, state `c8d7e4d67422ac6f`, crust `0bf1da504ef00e7a` |

## Neighbor Propagation Audits

| Fixture | Replay | Step | Active | Records | Non-boundary | Seed hits | Added | Dup | Rejected | Invalid | Max km | Hash |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| single_seed_growth | 0 | 0 | 2 | 2 | 1 | 1 | 1 | 0 | 0 | 0 | 158.461557 | `a1913514505f17e0` |
| single_seed_growth | 1 | 0 | 2 | 2 | 1 | 1 | 1 | 0 | 0 | 0 | 158.461557 | `a1913514505f17e0` |
| bounded_growth | 0 | 8 | 556 | 556 | 271 | 321 | 19 | 685 | 142 | 0 | 1799.844699 | `9438f78b3c23f526` |
| bounded_growth | 1 | 8 | 556 | 556 | 271 | 321 | 19 | 685 | 142 | 0 | 1799.844699 | `9438f78b3c23f526` |
| equal_age_ocean_ocean_deferred | 0 | 0 | 1 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0.000000 | `9c923a8b2381607e` |
| equal_age_ocean_ocean_deferred | 1 | 0 | 1 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0.000000 | `9c923a8b2381607e` |
| forced_divergence_empty | 0 | 1 | 362 | 362 | 0 | 0 | 0 | 0 | 0 | 0 | 239.999886 | `61ea4ce55aa40835` |
| forced_divergence_empty | 1 | 1 | 362 | 362 | 0 | 0 | 0 | 0 | 0 | 0 | 239.999886 | `61ea4ce55aa40835` |
| zero_motion_noop | 0 | 10 | 2421 | 2421 | 0 | 0 | 0 | 0 | 0 | 0 | 0.000000 | `33f1e105a5121513` |
| zero_motion_noop | 1 | 10 | 2421 | 2421 | 0 | 0 | 0 | 0 | 0 | 0 | 0.000000 | `33f1e105a5121513` |

## Notes

- The active list is still plate-local tracking state. It is not global sample ownership and is not material authority.
- Collision candidates and equal-age ocean-ocean deferrals do not propagate because they do not have a subducting under-plate yet.
- Newly added neighbors are bounded by parent distance plus plate-local triangle-barycenter distance; over-budget candidates are logged and rejected.

## Recommendation

IIIB.6 passes. Pause for user review before IIIB.7 (replay/event hash closure).
