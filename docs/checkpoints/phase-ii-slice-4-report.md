# Phase II Slice 4 Checkpoint: Material Accounting

Artifacts root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseII/Slice4/target_20260502`

This checkpoint adds a material ledger to the Slice 3 resampling filter event. It does not change carrier behavior, add terrain processes, or make unresolved contacts resolve. The goal is attribution: every authoritative continental/oceanic mass delta must reconcile to named records.

Audit equation: `active_after = active_before + single_hit_transfer + consumed_by_subduction + overwritten_by_gap_fill + unresolved_same_material + unresolved_triple_junction + unresolved_mixed_material + filter_exhausted_unknown + numeric_residual`.

## Gate Summary

| Fixture | Samples | Plates | Steps | Contact replay | Label replay | Filter replay | Material replay | Material log | Post-state replay | Global reconcile | Per-plate reconcile | No hidden change | Control stable | Expected subduction | Verdict |
|---|---:|---:|---:|---|---|---|---|---|---|---|---|---|---|---|---|
| cadence_60k_primary | 60000 | 40 | 32 | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass |
| forced_convergence_under_1 | 10000 | 2 | 40 | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass |
| forced_convergence_under_0 | 10000 | 2 | 40 | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass |
| forced_divergence_step0 | 10000 | 2 | 0 | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass |
| same_pair_mixed_signal | 10000 | 2 | 40 | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass |
| zero_motion | 10000 | 40 | 32 | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass |
| single_plate | 10000 | 1 | 32 | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass |
| all_continental_zero_motion | 10000 | 40 | 32 | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass |
| ocean_only_zero_motion | 10000 | 40 | 32 | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass |
| ocean_only_forced_convergence_under_1 | 10000 | 2 | 40 | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass |

## Material Ledger Metrics

| Fixture | Records | Material changed | Plate changed | Single-hit | Subduction | Gap fill | Non-sep gap | Unresolved same | Unresolved triple | Unresolved mixed | Filter exhausted | C before | C after | Ledger C delta | C residual | Max plate residual | Subduction loss | Gap-fill loss | Single-hit loss | Auth CAF before | Auth CAF after | Ledger hash |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| cadence_60k_primary | 59955 | 6904 | 45586 | 23967 | 2164 | 21273 | 10437 | 6478 | 5056 | 1017 | 0 | 3.783105873452 | 3.431566895588 | -0.351538977863 | 0.000000000000383 | 0.000000000000007 | 0.000655670207 | 0.235776528652 | 0.529485547632 | 0.301050000000 | 0.273075416991 | `ad1e2c0075ee2c52` |
| forced_convergence_under_1 | 9588 | 0 | 81 | 1 | 1984 | 4796 | 4692 | 2807 | 0 | 0 | 0 | 12.566370614361 | 12.566370614361 | -0.000000000000 | 0.000000000000000 | -0.000000000000012 | 0.000000000000 | 0.000000000000 | 0.000000000000 | 1.000000000000 | 1.000000000000 | `4b38a6b51f8a524a` |
| forced_convergence_under_0 | 9588 | 0 | 2065 | 1 | 1984 | 4796 | 4692 | 2807 | 0 | 0 | 0 | 12.566370614361 | 12.566370614361 | -0.000000000000 | 0.000000000000000 | 0.000000000000568 | 0.000000000000 | 0.000000000000 | 0.000000000000 | 1.000000000000 | 1.000000000000 | `205dac16815a7476` |
| forced_divergence_step0 | 105 | 0 | 21 | 21 | 0 | 0 | 0 | 84 | 0 | 0 | 0 | 6.286955218364 | 6.286955218364 | 0.000000000000 | -0.000000000000000 | -0.000000000000002 | 0.000000000000 | 0.000000000000 | 0.000000000000 | 0.500300000000 | 0.500300000000 | `473c32749bf11cbb` |
| same_pair_mixed_signal | 9588 | 0 | 658 | 1 | 3013 | 4791 | 4715 | 1783 | 0 | 0 | 0 | 12.566370614361 | 12.566370614361 | -0.000000000000 | 0.000000000000000 | 0.000000000000154 | 0.000000000000 | 0.000000000000 | 0.000000000000 | 1.000000000000 | 1.000000000000 | `6ad2d89446b45ddc` |
| zero_motion | 806 | 0 | 162 | 162 | 0 | 0 | 0 | 633 | 11 | 0 | 0 | 3.773681095492 | 3.773681095492 | 0.000000000000 | -0.000000000000000 | -0.000000000000000 | 0.000000000000 | 0.000000000000 | 0.000000000000 | 0.300300000000 | 0.300300000000 | `c044fe354640bff8` |
| single_plate | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0.000000000000 | 0.000000000000 | 0.000000000000 | 0.000000000000000 | 0.000000000000000 | 0.000000000000 | 0.000000000000 | 0.000000000000 | 0.000000000000 | 0.000000000000 | `469df79965a5d135` |
| all_continental_zero_motion | 806 | 0 | 162 | 162 | 0 | 0 | 0 | 633 | 11 | 0 | 0 | 12.566370614361 | 12.566370614361 | -0.000000000000 | 0.000000000000000 | 0.000000000000000 | 0.000000000000 | 0.000000000000 | 0.000000000000 | 1.000000000000 | 1.000000000000 | `21e27c15feda51ee` |
| ocean_only_zero_motion | 806 | 0 | 162 | 162 | 0 | 0 | 0 | 633 | 11 | 0 | 0 | 0.000000000000 | 0.000000000000 | 0.000000000000 | 0.000000000000000 | 0.000000000000000 | 0.000000000000 | 0.000000000000 | 0.000000000000 | 0.000000000000 | 0.000000000000 | `d6dce1c5d0c9ede2` |
| ocean_only_forced_convergence_under_1 | 9588 | 0 | 81 | 1 | 1984 | 4796 | 4692 | 2807 | 0 | 0 | 0 | 0.000000000000 | 0.000000000000 | 0.000000000000 | 0.000000000000000 | 0.000000000000000 | 0.000000000000 | 0.000000000000 | 0.000000000000 | 0.000000000000 | 0.000000000000 | `c7e9f09292389c8e` |

## Notes

- `cadence_60k_primary`: Primary attribution run: remaining CAF delta is now named by ledger categories, not hidden in aggregate CAF.
- `forced_convergence_under_1`: Fixture polarity enables subduction records; ledger must reconcile the resulting transfer.
- `forced_convergence_under_0`: Fixture polarity enables subduction records; ledger must reconcile the resulting transfer.
- `forced_divergence_step0`: No-material-change control.
- `same_pair_mixed_signal`: Fixture polarity enables subduction records; ledger must reconcile the resulting transfer.
- `zero_motion`: No-material-change control.
- `single_plate`: No-material-change control.
- `all_continental_zero_motion`: No-material-change control.
- `ocean_only_zero_motion`: No-material-change control.
- `ocean_only_forced_convergence_under_1`: No-material-change control.
- Slice 4 is an accounting slice. A CAF change is allowed only when the ledger names and reconciles it; it is not treated as proof that material conservation is solved.
- `material_ledger.jsonl` is the replay-sufficient material attribution artifact. The ledger hash is included in `metrics.jsonl` and the resampling filter metrics.
- The primary 60k run should now be read as a destruction-source breakdown: subduction filtering, q1/q2 gap fill, single-hit transfer, and unresolved classes are separate categories.

## Recommendation

Slice 4 material accounting gates pass. Pause for user review before Slice 5 scaling.
