# Phase III Slice IIIA.1 Checkpoint: Inert Elevation Field

Artifacts root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIA1/verify_60k_20260502`

This slice adds an inert `Elevation` scalar, in kilometers, to global samples and plate-local carrier vertices. It does not add uplift, collision, rifting, erosion, amplification, or any new mutation behavior. The existing Phase II `projection_hash`, `state_hash`, and material ledger are intentionally kept unchanged; `crust_state_hash` is an additive audit hash that includes `Elevation`.

## Gate Summary

| Gate | Result | Evidence |
|---|---|---|
| Projection replay hash | pass | `a411b6aad7877a55` vs `a411b6aad7877a55` |
| Phase II state replay hash | pass | `3b4a85366dab80db` vs `3b4a85366dab80db` |
| Phase II material ledger replay hash | pass | `bc3077100ba291b4` vs `bc3077100ba291b4` |
| New crust state replay hash | pass | `8e1bd41e6b0225b6` vs `8e1bd41e6b0225b6` |
| Elevation remains zero before/after resampling | pass | max sample 0.000e+00 km, max plate vertex 0.000e+00 km |
| Phase II baseline state hash unchanged | pass | baseline `3b4a85366dab80db`, IIIA.1 `3b4a85366dab80db` |
| Phase II baseline material ledger unchanged | pass | baseline `bc3077100ba291b4`, IIIA.1 `bc3077100ba291b4` |
| Phase II baseline projection hash | no baseline available | Slice 5.5 artifact did not record `projection_hash_after`; replay match above covers determinism for this slice. |

## Hashes

| Replay | Projection before | Projection after | State before | State after | Crust before | Crust after | Material ledger |
|---:|---|---|---|---|---|---|---|
| 0 | `c26b18220aec8c34` | `a411b6aad7877a55` | `b72cfe2cdc13e4a1` | `3b4a85366dab80db` | `edf515cf357b8546` | `8e1bd41e6b0225b6` | `bc3077100ba291b4` |
| 1 | `c26b18220aec8c34` | `a411b6aad7877a55` | `b72cfe2cdc13e4a1` | `3b4a85366dab80db` | `edf515cf357b8546` | `8e1bd41e6b0225b6` | `bc3077100ba291b4` |

## Elevation Audit

| Replay | Samples | Plate-local vertices | Max sample before km | Max plate vertex before km | Max sample after km | Max plate vertex after km |
|---:|---:|---:|---:|---:|---:|---:|
| 0 | 60000 | 67421 | 0.000e+00 | 0.000e+00 | 0.000e+00 | 0.000e+00 |
| 1 | 60000 | 67421 | 0.000e+00 | 0.000e+00 | 0.000e+00 | 0.000e+00 |

## Phase II Baseline

Baseline artifact: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseII/Slice55/verify_60k_20260502/metrics.jsonl`

| Metric | Baseline | IIIA.1 replay 0 | Result |
|---|---|---|---|
| State hash after resampling | `3b4a85366dab80db` | `3b4a85366dab80db` | pass |
| Material ledger hash | `bc3077100ba291b4` | `bc3077100ba291b4` | pass |

## Notes

- `Elevation` is stored on `FSphereSample` and copied into `FCarrierVertex` during plate-local triangulation rebuilds.
- No Phase IIIA.2+ fields or consumers are present in this slice.
- `state_hash` deliberately excludes `Elevation` so prior Phase II checkpoints stay comparable. `crust_state_hash` is the first Phase III field-aware hash.
- The resampling event used here is the accepted Phase II filtered resampling path after 32 rigid-motion steps at 60k/40/seed 42.

## Recommendation

IIIA.1 passes. Pause for user review before IIIA.2 (inert oceanic age).
