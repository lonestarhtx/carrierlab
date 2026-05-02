# Phase III Slice IIIA.2 Checkpoint: Inert Oceanic Age

Artifacts root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIA2/verify_60k_20260502`

This slice adds an inert `OceanicAge` scalar, in Ma, to global samples and plate-local carrier vertices. It does not generate ridge crust, increment age, subduct, collide, rift, erode, uplift, or amplify terrain. The existing Phase II `projection_hash`, `state_hash`, and material ledger stay unchanged; `crust_state_hash` is the additive Phase III field-aware hash and now includes both `Elevation` and `OceanicAge`.

## Gate Summary

| Gate | Result | Evidence |
|---|---|---|
| Projection replay hash | pass | `a411b6aad7877a55` vs `a411b6aad7877a55` |
| Phase II state replay hash | pass | `3b4a85366dab80db` vs `3b4a85366dab80db` |
| Phase II material ledger replay hash | pass | `bc3077100ba291b4` vs `bc3077100ba291b4` |
| Crust state replay hash includes `OceanicAge` | pass | `29d14205f7fe6fc4` vs `29d14205f7fe6fc4` |
| `Elevation` and `OceanicAge` remain zero before/after resampling | pass | max sample elevation 0.000e+00 km, max sample age 0.000e+00 Ma, max plate vertex age 0.000e+00 Ma |
| Phase II baseline state hash unchanged | pass | baseline `3b4a85366dab80db`, IIIA.2 `3b4a85366dab80db` |
| Phase II baseline material ledger unchanged | pass | baseline `bc3077100ba291b4`, IIIA.2 `bc3077100ba291b4` |
| Phase II baseline projection hash | no baseline available | Slice 5.5 artifact did not record `projection_hash_after`; replay match above covers determinism for this slice. |

## Hashes

| Replay | Projection before | Projection after | State before | State after | Crust before | Crust after | Material ledger |
|---:|---|---|---|---|---|---|---|
| 0 | `c26b18220aec8c34` | `a411b6aad7877a55` | `b72cfe2cdc13e4a1` | `3b4a85366dab80db` | `53db6125b5922756` | `29d14205f7fe6fc4` | `bc3077100ba291b4` |
| 1 | `c26b18220aec8c34` | `a411b6aad7877a55` | `b72cfe2cdc13e4a1` | `3b4a85366dab80db` | `53db6125b5922756` | `29d14205f7fe6fc4` | `bc3077100ba291b4` |

## Crust Field Audit

| Replay | Samples | Plate-local vertices | Max sample elevation before km | Max sample elevation after km | Max sample age before Ma | Max sample age after Ma | Max plate vertex age after Ma |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 0 | 60000 | 67421 | 0.000e+00 | 0.000e+00 | 0.000e+00 | 0.000e+00 | 0.000e+00 |
| 1 | 60000 | 67421 | 0.000e+00 | 0.000e+00 | 0.000e+00 | 0.000e+00 | 0.000e+00 |

## Phase II Baseline

Baseline artifact: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseII/Slice55/verify_60k_20260502/metrics.jsonl`

| Metric | Baseline | IIIA.2 replay 0 | Result |
|---|---|---|---|
| State hash after resampling | `3b4a85366dab80db` | `3b4a85366dab80db` | pass |
| Material ledger hash | `bc3077100ba291b4` | `bc3077100ba291b4` | pass |

## Notes

- `OceanicAge` is initialized to `0.0` on `FSphereSample` and copied into `FCarrierVertex` during plate-local triangulation rebuilds.
- Age generation at ridges belongs to IIIE; this slice only stores the field and proves it stays inert through the accepted Phase II filtered resampling path.
- `state_hash` deliberately excludes Phase III crust fields so prior Phase II checkpoints stay comparable. `crust_state_hash` now includes `Elevation` plus `OceanicAge`.
- The resampling event used here is the accepted Phase II filtered resampling path after 32 rigid-motion steps at 60k/40/seed 42.

## Recommendation

IIIA.2 passes. Pause for user review before IIIA.3 (inert vector fields with per-step rotation).
