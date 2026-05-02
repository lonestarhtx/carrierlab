# Phase III Slice IIIA.4 Checkpoint: Field Interpolation Through Resampling

Artifacts root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIA4/verify_60k_20260502`

This slice extends the accepted resampling paths to carry `Elevation`, `OceanicAge`, `RidgeDirection`, and `FoldDirection` through barycentric interpolation. Vector fields are interpolated component-wise, projected back into the target sample tangent plane, and normalized when non-zero. Gap-fill samples still receive zero Phase III fields; ridge generation populates them later in IIIE.

## Gate Summary

| Gate | Result | Evidence |
|---|---|---|
| Projection replay hash | pass | `a411b6aad7877a55` vs `a411b6aad7877a55` |
| Phase II state replay hash | pass | `3b4a85366dab80db` vs `3b4a85366dab80db` |
| Phase II material ledger replay hash | pass | `bc3077100ba291b4` vs `bc3077100ba291b4` |
| Crust state replay hash | pass | `a4e4e99de216c31c` vs `a4e4e99de216c31c` |
| Zero-field production run remains zero | pass | non-zero samples 0, non-zero plate vertices 0 |
| Boundary smear observed | pass | 29 smeared samples, elevation range 0.572750..10.000000 km |
| Boundary smear replay hash | pass | crust `867333924f292f10` vs `867333924f292f10` |
| Gap-fill fields remain zero | pass | 21273 gap-fill samples, max gap elevation 0.000e+00 km, max vector 0.000e+00 |
| Interpolated vectors remain tangent | pass | max radial dot 1.665e-16 |
| Phase II baseline state hash unchanged | pass | baseline `3b4a85366dab80db`, IIIA.4 `3b4a85366dab80db` |
| Phase II baseline material ledger unchanged | pass | baseline `bc3077100ba291b4`, IIIA.4 `bc3077100ba291b4` |

## Primary Zero-Field Replay

| Replay | Projection after | State after | Crust after | Material ledger | Non-zero samples | Non-zero plate vertices |
|---:|---|---|---|---|---:|---:|
| 0 | `a411b6aad7877a55` | `3b4a85366dab80db` | `a4e4e99de216c31c` | `bc3077100ba291b4` | 0 | 0 |
| 1 | `a411b6aad7877a55` | `3b4a85366dab80db` | `a4e4e99de216c31c` | `bc3077100ba291b4` | 0 | 0 |

## Boundary Smear Probe

| Replay | Seed plate | Seeded vertices | Zeroed vertices | Boundary tris | Smeared samples | Non-zero samples | Elevation min/mean/max km | Gap fills | Gap non-zero fields | Max vector radial dot | Crust after |
|---:|---:|---:|---:|---:|---:|---:|---|---:|---:|---:|---|
| 0 | 0 | 1422 | 109 | 153 | 29 | 789 | 0.572750 / 9.872198 / 10.000000 | 21273 | 0 | 1.665e-16 | `867333924f292f10` |
| 1 | 0 | 1422 | 109 | 153 | 29 | 789 | 0.572750 / 9.872198 / 10.000000 | 21273 | 0 | 1.665e-16 | `867333924f292f10` |

## Phase II Baseline

Baseline artifact: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseII/Slice55/verify_60k_20260502/metrics.jsonl`

| Metric | Baseline | IIIA.4 replay 0 | Result |
|---|---|---|---|
| State hash after resampling | `3b4a85366dab80db` | `3b4a85366dab80db` | pass |
| Material ledger hash | `bc3077100ba291b4` | `bc3077100ba291b4` | pass |

## Notes

- Valid single-hit resampling now copies all Phase IIIA crust fields from the hit plate-local triangle by barycentric interpolation.
- The synthetic boundary-smear fixture seeds one side of a boundary triangle on one plate-local mesh and leaves the opposite-side boundary vertices at zero, so fractional post-resample values are expected evidence of interpolation rather than value passthrough.
- Gap-fill samples are explicitly zeroed for Phase III fields. IIIE will populate ridge direction, oceanic age, and ridge elevation as the paper's divergent-zone process.
- Phase II `state_hash` and material ledger remain comparable because they intentionally exclude Phase IIIA crust fields.

## Recommendation

IIIA.4 passes. Pause for user review before IIIA consolidation.
