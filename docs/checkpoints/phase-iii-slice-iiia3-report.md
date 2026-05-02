# Phase III Slice IIIA.3 Checkpoint: Inert Vector Fields With Per-Step Rotation

Artifacts root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIA3/verify_60k_20260502`

This slice adds inert tangent vector storage for `RidgeDirection` and `FoldDirection` on global samples and plate-local vertices. Production initialization remains zero. The only behavior added is that non-zero plate-local vectors rotate with the same per-step geodetic transform as their vertex positions; no ridge generation, folding, collision, rifting, uplift, erosion, amplification, or material mutation is added.

## Gate Summary

| Gate | Result | Evidence |
|---|---|---|
| Projection replay hash | pass | `a411b6aad7877a55` vs `a411b6aad7877a55` |
| Phase II state replay hash | pass | `3b4a85366dab80db` vs `3b4a85366dab80db` |
| Phase II material ledger replay hash | pass | `bc3077100ba291b4` vs `bc3077100ba291b4` |
| Crust state replay hash includes vector fields | pass | `a4e4e99de216c31c` vs `a4e4e99de216c31c` |
| Production vector fields remain zero through Phase II resampling | pass | max sample 0.000e+00, max plate vertex 0.000e+00 |
| Forced vector rotation oracle | pass | ridge 2.719e-16 rad / fold 4.996e-16 rad / position 5.495e-16 rad |
| Forced vector replay hash | pass | crust `a39d037a6b669e74` vs `a39d037a6b669e74` |
| Phase II baseline state hash unchanged | pass | baseline `3b4a85366dab80db`, IIIA.3 `3b4a85366dab80db` |
| Phase II baseline material ledger unchanged | pass | baseline `bc3077100ba291b4`, IIIA.3 `bc3077100ba291b4` |

## Primary Hashes

| Replay | Projection before | Projection after | State before | State after | Crust before | Crust after | Material ledger |
|---:|---|---|---|---|---|---|---|
| 0 | `c26b18220aec8c34` | `a411b6aad7877a55` | `b72cfe2cdc13e4a1` | `3b4a85366dab80db` | `7495150f65cb4176` | `a4e4e99de216c31c` | `bc3077100ba291b4` |
| 1 | `c26b18220aec8c34` | `a411b6aad7877a55` | `b72cfe2cdc13e4a1` | `3b4a85366dab80db` | `7495150f65cb4176` | `a4e4e99de216c31c` | `bc3077100ba291b4` |

## Zero-Vector Audit

| Replay | Samples | Plate-local vertices | Max sample vector before | Max sample vector after | Max plate vector before | Max plate vector after | Max plate radial dot after |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 0 | 60000 | 67421 | 0.000e+00 | 0.000e+00 | 0.000e+00 | 0.000e+00 | 0.000e+00 |
| 1 | 60000 | 67421 | 0.000e+00 | 0.000e+00 | 0.000e+00 | 0.000e+00 | 0.000e+00 |

## Forced-Rotation Probe

| Replay | Plate | Local vertex | Steps | Position err rad | Ridge err rad | Fold err rad | Ridge mag err | Fold mag err | Ridge radial dot | Fold radial dot | Crust after |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| 0 | 0 | 0 | 5 | 5.495e-16 | 2.719e-16 | 4.996e-16 | 9.992e-16 | 4.441e-16 | 9.714e-17 | 9.437e-16 | `a39d037a6b669e74` |
| 1 | 0 | 0 | 5 | 5.495e-16 | 2.719e-16 | 4.996e-16 | 9.992e-16 | 4.441e-16 | 9.714e-17 | 9.437e-16 | `a39d037a6b669e74` |

## Phase II Baseline

Baseline artifact: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseII/Slice55/verify_60k_20260502/metrics.jsonl`

| Metric | Baseline | IIIA.3 replay 0 | Result |
|---|---|---|---|
| State hash after resampling | `3b4a85366dab80db` | `3b4a85366dab80db` | pass |
| Material ledger hash | `bc3077100ba291b4` | `bc3077100ba291b4` | pass |

## Notes

- `RidgeDirection` and `FoldDirection` are initialized to zero on `FSphereSample` and copied into `FCarrierVertex` during plate-local triangulation rebuilds.
- Zero vectors are left zero during per-step rotation; non-zero vector fields rotate with the same axis and angular speed as their plate-local vertex positions.
- The forced-rotation oracle uses `FQuat4d` recomputation from the initial vector, axis, and elapsed angle rather than reading the implementation's rotated value as expected truth.
- `state_hash` deliberately excludes Phase III crust fields so prior Phase II checkpoints stay comparable. `crust_state_hash` now includes `Elevation`, `OceanicAge`, `RidgeDirection`, and `FoldDirection`.

## Recommendation

IIIA.3 passes. Pause for user review before IIIA.4 (field interpolation through resampling).
