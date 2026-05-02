# Phase III Slice IIIB.3 Checkpoint: Subduction Matrix

Artifacts root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIB3/20260502T080305Z`

This slice adds a sparse per-remesh-window subduction matrix as read-only convergence tracking state. Active boundary triangle barycenters cast ray-from-origin queries against the other plates' BVHs; non-boundary intersections add a canonical plate-pair flag only when the plate pair's signed convergence is positive. The matrix resets at remesh and does not mark triangles, filter projection candidates, or mutate material.

## Gate Summary

| Gate | Result | Evidence |
|---|---|---|
| Matrix empty at start of remesh window | pass | initial pairs 0 / 0 |
| Matrix resets after remesh | pass | pre pairs 66 -> post pairs 0, serial 0 -> 1 |
| Matrix audit has no invalid/self pairs | pass | invalid 0, self 0 |
| Matrix replay deterministic | pass | pre `cedddf054c070679` vs `cedddf054c070679`, post `6011fd48f0a8730c` vs `6011fd48f0a8730c` |
| Forced convergence populates expected plate pair | pass | pairs 1, probe 0/1, signed convergence 0.075341390677 |
| Forced divergence leaves matrix empty | pass | pairs 0, non-convergent hits 160 |
| Zero-motion leaves matrix empty and hash stable | pass | pairs 0 -> 0, hash `33065163436c1d40` -> `33065163436c1d40` |
| Control replay hashes deterministic | pass | convergence `de372a63d1d42c26` vs `de372a63d1d42c26`, divergence `bc4d0fba206e794a` vs `bc4d0fba206e794a`, zero `33065163436c1d40` vs `33065163436c1d40` |
| Projection replay hash | pass | `a411b6aad7877a55` vs `a411b6aad7877a55` |
| Phase II state replay hash | pass | `3b4a85366dab80db` vs `3b4a85366dab80db` |
| Crust state replay hash | pass | `a4e4e99de216c31c` vs `a4e4e99de216c31c` |
| Phase II material ledger replay hash | pass | `bc3077100ba291b4` vs `bc3077100ba291b4` |
| Phase II baseline state hash unchanged | pass | baseline `3b4a85366dab80db`, IIIB.3 `3b4a85366dab80db` |
| Phase II baseline material ledger unchanged | pass | baseline `bc3077100ba291b4`, IIIB.3 `bc3077100ba291b4` |

## Matrix Audits

| Fixture | Replay | Window | Step | Reset | Active | Pairs | Ray tests | Hits | Boundary hits | Non-convergent hits | Probe pair | Probe signed convergence | Hash |
|---|---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---|---:|---|
| baseline | 0 | initial | 0 | 0 | 6002 | 0 | 0 | 0 | 0 | 0 | -1/-1 | 0.000000000000 | `4cc376544a579cf8` |
| baseline | 0 | pre-remesh | 32 | 0 | 0 | 66 | 2369856 | 36488 | 0 | 6658 | 0/2 | 0.000299892228 | `cedddf054c070679` |
| baseline | 0 | post-remesh | 32 | 1 | 9792 | 0 | 0 | 0 | 0 | 0 | -1/-1 | 0.000000000000 | `6011fd48f0a8730c` |
| forced_convergence_matrix | 0 | initial | 0 | 0 | 362 | 0 | 0 | 0 | 0 | 0 | -1/-1 | 0.000000000000 | `d126df2cf58bac31` |
| forced_convergence_matrix | 0 | final | 4 | 0 | 362 | 1 | 1448 | 711 | 0 | 0 | 0/1 | 0.075341390677 | `de372a63d1d42c26` |
| forced_divergence_matrix | 0 | initial | 0 | 0 | 362 | 0 | 0 | 0 | 0 | 0 | -1/-1 | 0.000000000000 | `d126df2cf58bac31` |
| forced_divergence_matrix | 0 | final | 1 | 0 | 362 | 0 | 362 | 0 | 0 | 160 | -1/-1 | 0.000000000000 | `bc4d0fba206e794a` |
| zero_motion_matrix | 0 | initial | 0 | 0 | 2421 | 0 | 0 | 0 | 0 | 0 | -1/-1 | 0.000000000000 | `33065163436c1d40` |
| zero_motion_matrix | 0 | final | 10 | 0 | 2421 | 0 | 944190 | 0 | 0 | 0 | -1/-1 | 0.000000000000 | `33065163436c1d40` |

## Phase II Baseline

Baseline artifact: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseII/Slice55/verify_60k_20260502/metrics.jsonl`

| Metric | Baseline | IIIB.3 replay 0 | Result |
|---|---|---|---|
| State hash after resampling | `3b4a85366dab80db` | `3b4a85366dab80db` | pass |
| Material ledger hash | `bc3077100ba291b4` | `bc3077100ba291b4` | pass |

## Notes

- `SubductionMatrix` is a plate-pair evidence matrix only. It does not assign under/over polarity; IIIB.4 owns that decision.
- Boundary-degenerate hits are counted but do not populate the matrix. Ray hits from plate pairs with non-positive pair-level signed convergence are counted and rejected.
- The pair-level convergence gate prevents the two-plate forced-divergence control from authorizing antipodal backside intersections as ordinary subduction matrix entries.
- Matrix state is reset by the same plate-local rebuild path that resets the active list and distance-to-front values.
- Phase II `state_hash` and `material_ledger_hash` remain matched to the Slice 5.5 baseline, so the new matrix has not changed filter or material behavior.

## Recommendation

IIIB.3 passes. Pause for user review before IIIB.4 (oceanic-under-continental polarity rule).
