# Phase III Slice IIIB.3 Checkpoint: Subduction Matrix

Artifacts root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIB3/20260506T205248Z`

This slice adds a sparse per-remesh-window subduction matrix as read-only convergence tracking state. Active boundary triangle barycenters cast ray-from-origin queries against the other plates' BVHs; non-boundary intersections add a canonical plate-pair flag only when the plate pair's signed convergence is positive. The matrix resets at remesh and does not mark triangles, filter projection candidates, or mutate material.

## Gate Summary

| Gate | Result | Evidence |
|---|---|---|
| Matrix empty at start of remesh window | pass | initial pairs 0 / 0 |
| Matrix resets after remesh | pass | pre pairs 75 -> post pairs 0, serial 0 -> 1 |
| Matrix audit has no invalid/self pairs | pass | invalid 0, self 0 |
| Matrix replay deterministic | pass | pre `d145f51eab69d5d8` vs `d145f51eab69d5d8`, post `60a0688dd0b05b59` vs `60a0688dd0b05b59` |
| Forced convergence populates expected plate pair | pass | pairs 1, probe 0/1, signed convergence 0.024302885046 |
| Forced divergence admits only local convergent evidence | pass | pairs 1, hits 160, probe local sign 0.073874712034 |
| Zero-motion leaves matrix empty and hash stable | pass | pairs 0 -> 0, hash `33f1e105a5121513` -> `33f1e105a5121513` |
| Control replay hashes deterministic | pass | convergence `afbc166517b8f53d` vs `afbc166517b8f53d`, divergence `34ec2a2505c2ae8d` vs `34ec2a2505c2ae8d`, zero `33f1e105a5121513` vs `33f1e105a5121513` |
| Projection replay hash | pass | `a411b6aad7877a55` vs `a411b6aad7877a55` |
| Phase II state replay hash | pass | `3b4a85366dab80db` vs `3b4a85366dab80db` |
| Crust state replay hash | pass | `a4e4e99de216c31c` vs `a4e4e99de216c31c` |
| Phase II material ledger replay hash | pass | `bc3077100ba291b4` vs `bc3077100ba291b4` |
| Phase II baseline state hash unchanged | pass | baseline `3b4a85366dab80db`, IIIB.3 `3b4a85366dab80db` |
| Phase II baseline material ledger unchanged | pass | baseline `bc3077100ba291b4`, IIIB.3 `bc3077100ba291b4` |

## Matrix Audits

| Fixture | Replay | Window | Step | Reset | Active | Pairs | Ray tests | Hits | Boundary hits | Non-convergent hits | Probe pair | Probe signed convergence | Hash |
|---|---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---|---:|---|
| baseline | 0 | initial | 0 | 0 | 6002 | 0 | 0 | 0 | 0 | 0 | -1/-1 | 0.000000000000 | `6617ce5e484d4477` |
| baseline | 0 | pre-remesh | 32 | 0 | 0 | 75 | 2777516 | 39437 | 0 | 17154 | 0/2 | 0.000000000000 | `d145f51eab69d5d8` |
| baseline | 0 | post-remesh | 32 | 1 | 9792 | 0 | 0 | 0 | 0 | 0 | -1/-1 | 0.000000000000 | `60a0688dd0b05b59` |
| forced_convergence_matrix | 0 | initial | 0 | 0 | 362 | 0 | 0 | 0 | 0 | 0 | -1/-1 | 0.000000000000 | `2bd0e26181fad2fe` |
| forced_convergence_matrix | 0 | final | 4 | 0 | 712 | 1 | 1968 | 1224 | 0 | 0 | 0/1 | 0.024302885046 | `afbc166517b8f53d` |
| forced_divergence_matrix | 0 | initial | 0 | 0 | 362 | 0 | 0 | 0 | 0 | 0 | -1/-1 | 0.000000000000 | `2bd0e26181fad2fe` |
| forced_divergence_matrix | 0 | final | 1 | 0 | 442 | 1 | 362 | 160 | 0 | 0 | 0/1 | 0.073874712034 | `34ec2a2505c2ae8d` |
| zero_motion_matrix | 0 | initial | 0 | 0 | 2421 | 0 | 0 | 0 | 0 | 0 | -1/-1 | 0.000000000000 | `33f1e105a5121513` |
| zero_motion_matrix | 0 | final | 10 | 0 | 2421 | 0 | 944190 | 0 | 0 | 0 | -1/-1 | 0.000000000000 | `33f1e105a5121513` |

## Phase II Baseline

Baseline artifact: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseII/Slice55/verify_60k_20260502/metrics.jsonl`

| Metric | Baseline | IIIB.3 replay 0 | Result |
|---|---|---|---|
| State hash after resampling | `3b4a85366dab80db` | `3b4a85366dab80db` | pass |
| Material ledger hash | `bc3077100ba291b4` | `bc3077100ba291b4` | pass |

## Notes

- `SubductionMatrix` is a plate-pair evidence matrix only. It does not assign under/over polarity; IIIB.4 owns that decision.
- Boundary-degenerate hits are counted but do not populate the matrix. Ray hits with non-positive local signed convergence at the active triangle barycenter are counted and rejected.
- The forced-divergence fixture can still produce locally convergent backside intersections on a closed sphere; matrix admission is local evidence, not blanket pair-wide classification.
- Matrix state is reset by the same plate-local rebuild path that resets the active list and distance-to-front values.
- Phase II `state_hash` and `material_ledger_hash` remain matched to the Slice 5.5 baseline, so the new matrix has not changed filter or material behavior.

## Recommendation

IIIB.3 passes. Pause for user review before IIIB.4 (oceanic-under-continental polarity rule).
