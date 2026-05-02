# Phase III Slice IIIB.2 Checkpoint: Per-Triangle Distance-To-Front

Artifacts root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIB2/20260502T074653Z`

This slice adds `DistanceToFront` as a per-active-triangle kilometer scalar. It is read-only tracking state: it does not modify crust fields, projection ownership, Phase II contact detection, triangle labels, or resampling filters. Distances initialize to 0 at remesh-window creation, advance each rigid step by the thesis over-estimation `d(p,t+dt)=d(p,t)+s(p)dt`, and active triangles are culled when `d > r_s = 1800 km`.

## Gate Summary

| Gate | Result | Evidence |
|---|---|---|
| Initial distance records cover active triangles | pass | records 6002, active 6002, missing 0 |
| Distance records remain finite/nonnegative/below threshold pre-remesh | pass | nonfinite 0, negative 0, over-threshold active 0 |
| Distance records reset cleanly after remesh | pass | serial 0 -> 1, max distance 0.000000 km |
| Distance trajectory replay deterministic | pass | baseline pre `95375abef3459fa2` vs `95375abef3459fa2`, analytic final `39489cc5905c478a` vs `39489cc5905c478a` |
| Forced-convergence probe evolves analytically | pass | expected 942.438165862 km, observed 942.438165862 km, step 235.609541465 km x 4 |
| Active list shrinks past r_s | pass | active 362 -> 152, culled 210, threshold 1800.0 km |
| Zero-motion tracking no-op | pass | active 2421 -> 2421, hash `1a1cb9e492637f0b` -> `1a1cb9e492637f0b` |
| Projection replay hash | pass | `a411b6aad7877a55` vs `a411b6aad7877a55` |
| Phase II state replay hash | pass | `3b4a85366dab80db` vs `3b4a85366dab80db` |
| Crust state replay hash | pass | `a4e4e99de216c31c` vs `a4e4e99de216c31c` |
| Phase II material ledger replay hash | pass | `bc3077100ba291b4` vs `bc3077100ba291b4` |
| Phase II baseline state hash unchanged | pass | baseline `3b4a85366dab80db`, IIIB.2 `3b4a85366dab80db` |
| Phase II baseline material ledger unchanged | pass | baseline `bc3077100ba291b4`, IIIB.2 `bc3077100ba291b4` |

## Distance Audits

| Fixture | Replay | Window | Step | Reset | Active | Records | Culled | Min km | Mean km | Max km | Probe step km | Probe total km | Hash |
|---|---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| baseline | 0 | initial | 0 | 0 | 6002 | 6002 | 0 | 0.000000 | 0.000000 | 0.000000 | 132.685583 | 0.000000 | `ea0b1bd7732e950f` |
| baseline | 0 | pre-remesh | 32 | 0 | 0 | 0 | 6002 | 0.000000 | 0.000000 | 0.000000 | 0.000000 | 0.000000 | `95375abef3459fa2` |
| baseline | 0 | post-remesh | 32 | 1 | 9792 | 9792 | 0 | 0.000000 | 0.000000 | 0.000000 | 123.425559 | 0.000000 | `c687aba6d9c82c61` |
| forced_convergence_analytic | 0 | initial | 0 | 0 | 362 | 362 | 0 | 0.000000 | 0.000000 | 0.000000 | 235.609541 | 0.000000 | `cb81d0e00da50b76` |
| forced_convergence_analytic | 0 | final | 4 | 0 | 362 | 362 | 0 | 3.566317 | 617.195594 | 959.999545 | 235.609541 | 942.438166 | `39489cc5905c478a` |
| forced_convergence_cull | 0 | initial | 0 | 0 | 362 | 362 | 0 | 0.000000 | 0.000000 | 0.000000 | 235.609541 | 0.000000 | `cb81d0e00da50b76` |
| forced_convergence_cull | 0 | final | 12 | 0 | 152 | 152 | 210 | 10.698950 | 958.042518 | 1778.398094 | 89.787518 | 1077.450220 | `5f4367270c1d8ee8` |
| zero_motion_noop | 0 | initial | 0 | 0 | 2421 | 2421 | 0 | 0.000000 | 0.000000 | 0.000000 | 0.000000 | 0.000000 | `1a1cb9e492637f0b` |
| zero_motion_noop | 0 | final | 10 | 0 | 2421 | 2421 | 0 | 0.000000 | 0.000000 | 0.000000 | 0.000000 | 0.000000 | `1a1cb9e492637f0b` |

## Phase II Baseline

Baseline artifact: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseII/Slice55/verify_60k_20260502/metrics.jsonl`

| Metric | Baseline | IIIB.2 replay 0 | Result |
|---|---|---|---|
| State hash after resampling | `3b4a85366dab80db` | `3b4a85366dab80db` | pass |
| Material ledger hash | `bc3077100ba291b4` | `bc3077100ba291b4` | pass |

## Notes

- Distance-to-front is process tracking state with remesh-window lifetime. It is not global ownership, not projection authority, and not a material mutation.
- The distance increment uses local triangle barycenter tangential speed under the current plate geodetic rotation, matching the thesis over-estimation form and avoiding brute-force exact distance recomputation.
- Culling only removes entries from the active convergence tracking list. It does not remove plate-local triangles from the carrier mesh.
- `state_hash` and `material_ledger_hash` match the Phase II Slice 5.5 baseline, confirming this read-only tracking state did not change filter or material behavior.

## Recommendation

IIIB.2 passes. Pause for user review before IIIB.3 (subduction matrix).
