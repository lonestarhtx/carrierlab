# Phase II Slice 5 Checkpoint: Resolution Scaling

Artifacts root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseII/Slice5/target_20260502`

This checkpoint runs the accepted Slice 3+4 contact-label-filter-material-accounting stack across 60k, 100k, 250k, and 500k samples. It does not add new process behavior. The purpose is to test whether the Slice 4 audit equation and destruction-source breakdown remain stable as sample density increases.

Audit equation: `active_after = active_before + single_hit_transfer + consumed_by_subduction + overwritten_by_gap_fill + unresolved_same_material + unresolved_triple_junction + unresolved_mixed_material + filter_exhausted_unknown + numeric_residual`.

## Gate Summary

| Fixture | Family | Samples | Plates | Steps | Contact replay | Label replay | Filter replay | Material replay | Post-state replay | Global reconcile | Per-plate reconcile | Control stable | Expected subduction | Runtime | Verdict |
|---|---|---:|---:|---:|---|---|---|---|---|---|---|---|---|---|---|
| cadence_60k_primary | primary | 60000 | 40 | 32 | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass |
| 60k_forced_convergence_under_1 | forced_convergence_under_1 | 60000 | 2 | 40 | pass | pass | pass | pass | pass | pass | pass | pass | pass | control | pass |
| 60k_forced_convergence_under_0 | forced_convergence_under_0 | 60000 | 2 | 40 | pass | pass | pass | pass | pass | pass | pass | pass | pass | control | pass |
| 60k_forced_divergence_step0 | forced_divergence_step0 | 60000 | 2 | 0 | pass | pass | pass | pass | pass | pass | pass | pass | pass | control | pass |
| 60k_same_pair_mixed_signal | same_pair_mixed_signal | 60000 | 2 | 40 | pass | pass | pass | pass | pass | pass | pass | pass | pass | control | pass |
| 60k_zero_motion | zero_motion | 60000 | 40 | 32 | pass | pass | pass | pass | pass | pass | pass | pass | pass | control | pass |
| 60k_single_plate | single_plate | 60000 | 1 | 32 | pass | pass | pass | pass | pass | pass | pass | pass | pass | control | pass |
| 60k_all_continental_zero_motion | all_continental_zero_motion | 60000 | 40 | 32 | pass | pass | pass | pass | pass | pass | pass | pass | pass | control | pass |
| 60k_ocean_only_zero_motion | ocean_only_zero_motion | 60000 | 40 | 32 | pass | pass | pass | pass | pass | pass | pass | pass | pass | control | pass |
| 60k_ocean_only_forced_convergence_under_1 | ocean_only_forced_convergence_under_1 | 60000 | 2 | 40 | pass | pass | pass | pass | pass | pass | pass | pass | pass | control | pass |
| cadence_100k_primary | primary | 100000 | 40 | 32 | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass |
| 100k_forced_convergence_under_1 | forced_convergence_under_1 | 100000 | 2 | 40 | pass | pass | pass | pass | pass | pass | pass | pass | pass | control | pass |
| 100k_forced_convergence_under_0 | forced_convergence_under_0 | 100000 | 2 | 40 | pass | pass | pass | pass | pass | pass | pass | pass | pass | control | pass |
| 100k_forced_divergence_step0 | forced_divergence_step0 | 100000 | 2 | 0 | pass | pass | pass | pass | pass | pass | pass | pass | pass | control | pass |
| 100k_same_pair_mixed_signal | same_pair_mixed_signal | 100000 | 2 | 40 | pass | pass | pass | pass | pass | pass | pass | pass | pass | control | pass |
| 100k_zero_motion | zero_motion | 100000 | 40 | 32 | pass | pass | pass | pass | pass | pass | pass | pass | pass | control | pass |
| 100k_single_plate | single_plate | 100000 | 1 | 32 | pass | pass | pass | pass | pass | pass | pass | pass | pass | control | pass |
| 100k_all_continental_zero_motion | all_continental_zero_motion | 100000 | 40 | 32 | pass | pass | pass | pass | pass | pass | pass | pass | pass | control | pass |
| 100k_ocean_only_zero_motion | ocean_only_zero_motion | 100000 | 40 | 32 | pass | pass | pass | pass | pass | pass | pass | pass | pass | control | pass |
| 100k_ocean_only_forced_convergence_under_1 | ocean_only_forced_convergence_under_1 | 100000 | 2 | 40 | pass | pass | pass | pass | pass | pass | pass | pass | pass | control | pass |
| cadence_250k_primary | primary | 250000 | 40 | 32 | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass |
| 250k_forced_convergence_under_1 | forced_convergence_under_1 | 250000 | 2 | 40 | pass | pass | pass | pass | pass | pass | pass | pass | pass | control | pass |
| 250k_forced_convergence_under_0 | forced_convergence_under_0 | 250000 | 2 | 40 | pass | pass | pass | pass | pass | pass | pass | pass | pass | control | pass |
| 250k_forced_divergence_step0 | forced_divergence_step0 | 250000 | 2 | 0 | pass | pass | pass | pass | pass | pass | pass | pass | pass | control | pass |
| 250k_same_pair_mixed_signal | same_pair_mixed_signal | 250000 | 2 | 40 | pass | pass | pass | pass | pass | pass | pass | pass | pass | control | pass |
| 250k_zero_motion | zero_motion | 250000 | 40 | 32 | pass | pass | pass | pass | pass | pass | pass | pass | pass | control | pass |
| 250k_single_plate | single_plate | 250000 | 1 | 32 | pass | pass | pass | pass | pass | pass | pass | pass | pass | control | pass |
| 250k_all_continental_zero_motion | all_continental_zero_motion | 250000 | 40 | 32 | pass | pass | pass | pass | pass | pass | pass | pass | pass | control | pass |
| 250k_ocean_only_zero_motion | ocean_only_zero_motion | 250000 | 40 | 32 | pass | pass | pass | pass | pass | pass | pass | pass | pass | control | pass |
| 250k_ocean_only_forced_convergence_under_1 | ocean_only_forced_convergence_under_1 | 250000 | 2 | 40 | pass | pass | pass | pass | pass | pass | pass | pass | pass | control | pass |
| cadence_500k_primary | primary | 500000 | 40 | 32 | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass |
| 500k_forced_convergence_under_1 | forced_convergence_under_1 | 500000 | 2 | 40 | pass | pass | pass | pass | pass | pass | pass | pass | pass | control | pass |
| 500k_forced_convergence_under_0 | forced_convergence_under_0 | 500000 | 2 | 40 | pass | pass | pass | pass | pass | pass | pass | pass | pass | control | pass |
| 500k_forced_divergence_step0 | forced_divergence_step0 | 500000 | 2 | 0 | pass | pass | pass | pass | pass | pass | pass | pass | pass | control | pass |
| 500k_same_pair_mixed_signal | same_pair_mixed_signal | 500000 | 2 | 40 | pass | pass | pass | pass | pass | pass | pass | pass | pass | control | pass |
| 500k_zero_motion | zero_motion | 500000 | 40 | 32 | pass | pass | pass | pass | pass | pass | pass | pass | pass | control | pass |
| 500k_single_plate | single_plate | 500000 | 1 | 32 | pass | pass | pass | pass | pass | pass | pass | pass | pass | control | pass |
| 500k_all_continental_zero_motion | all_continental_zero_motion | 500000 | 40 | 32 | pass | pass | pass | pass | pass | pass | pass | pass | pass | control | pass |
| 500k_ocean_only_zero_motion | ocean_only_zero_motion | 500000 | 40 | 32 | pass | pass | pass | pass | pass | pass | pass | pass | pass | control | pass |
| 500k_ocean_only_forced_convergence_under_1 | ocean_only_forced_convergence_under_1 | 500000 | 2 | 40 | pass | pass | pass | pass | pass | pass | pass | pass | pass | control | pass |

## Primary Scaling Metrics

| Resolution | Avg step kernel s | Avg step wall s | Paper Table 2 s | Contact s | Label s | Filter event s | Total replay s | Auth CAF before | Auth CAF after | Projected CAF before | Projected CAF after | Runtime |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| 60000 | 0.032305 | 0.040805 | 0.19 | 0.171097 | 0.003184 | 0.891983 | 2.758500 | 0.301050000000 | 0.273075416991 | 0.191342288749 | 0.273075416991 | pass |
| 100000 | 0.058505 | 0.073577 | 0.28 | 0.291058 | 0.005730 | 1.549155 | 4.832297 | 0.301040000000 | 0.274937409838 | 0.191610795296 | 0.274937409838 | pass |
| 250000 | 0.141558 | 0.173858 | 1.24 | 0.766808 | 0.012041 | 4.801562 | 12.729793 | 0.301056000000 | 0.274440107899 | 0.191540763189 | 0.274440107899 | pass |
| 500000 | 0.301934 | 0.367856 | 1.90 | 1.573589 | 0.024140 | 14.074536 | 30.940227 | 0.301078000000 | 0.274776406898 | 0.191739569842 | 0.274776406898 | pass |

## Primary Destruction Breakdown

| Resolution | Net C delta | Single-hit loss/gain/net | Subduction loss/gain/net | Gap-fill loss/gain/net | Unresolved same loss/gain/net | Unresolved triple loss/gain/net | Unresolved mixed loss/gain/net | Gap-fill net % of net loss | Single-hit net % of net loss |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 60000 | -0.351538977863 | 0.529486 / 0.154508 / -0.374977 | 0.000656 / 0.177194 / 0.176538 | 0.235777 / 0.082676 / -0.153100 | 0.000000 / 0.000000 / 0.000000 | 0.000000 / 0.000000 / 0.000000 | 0.000000 / 0.000000 / 0.000000 | 43.55% | 106.67% |
| 100000 | -0.328014821975 | 0.523571 / 0.155556 / -0.368014 | 0.000352 / 0.178864 / 0.178512 | 0.225629 / 0.087116 / -0.138513 | 0.000000 / 0.000000 / 0.000000 | 0.000000 / 0.000000 / 0.000000 | 0.000000 / 0.000000 / 0.000000 | 42.23% | 112.19% |
| 250000 | -0.334465164381 | 0.525947 / 0.158025 / -0.367922 | 0.000244 / 0.180539 / 0.180295 | 0.227275 / 0.080437 / -0.146838 | 0.000000 / 0.000000 / 0.000000 | 0.000000 / 0.000000 / 0.000000 | 0.000000 / 0.000000 / 0.000000 | 43.90% | 110.00% |
| 500000 | -0.330515566667 | 0.525065 / 0.158176 / -0.366889 | 0.000194 / 0.179875 / 0.179681 | 0.228105 / 0.084798 / -0.143307 | 0.000000 / 0.000000 / 0.000000 | 0.000000 / 0.000000 / 0.000000 | 0.000000 / 0.000000 / 0.000000 | 43.36% | 111.01% |

## Primary Count Breakdown

| Resolution | Contacts | Third-plate contacts | Labels | Material records | Material changed | Plate changed | Subduction | Gap fill | Non-sep gap | Unresolved same | Unresolved triple | Unresolved mixed | C residual | Max plate residual | Ledger hash |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| 60000 | 29472 | 19813 | 10328 | 59955 | 6904 | 45586 | 2164 | 21273 | 10437 | 6478 | 5056 | 1017 | 3.830e-13 | 7.000e-15 | `ad1e2c0075ee2c52` |
| 100000 | 49164 | 33020 | 17318 | 99929 | 11342 | 75973 | 3628 | 35506 | 17301 | 10810 | 8456 | 1706 | 4.550e-13 | 3.300e-14 | `6eb1861115689611` |
| 250000 | 122869 | 82565 | 43274 | 249814 | 28172 | 189952 | 9076 | 88748 | 43107 | 27022 | 21139 | 4206 | 1.048e-12 | 1.090e-13 | `c45888d33d2e6be1` |
| 500000 | 245677 | 165061 | 86492 | 499594 | 56271 | 379802 | 18137 | 177340 | 86463 | 54044 | 42261 | 8435 | 1.342e-12 | 5.600e-14 | `83a38cc3341d3186` |

## Notes

- Runtime `finding` means the average per-step projection kernel exceeded the paper Table 2 total for that resolution. It is reported as a scaling finding, not hidden by the gate labels.
- Gross loss/gain/net are reported together. Category priority should be based on net contribution and mechanism, not gross loss alone.
- Material reconciliation uses `max(1e-12, total_area * 1e-12)` as the scaling gate. The residuals remain printed so floating-point summation noise cannot hide a real leak.
- Control fixtures are scaled with the target resolution in this commandlet so replay, label, filter, material, and no-material-change gates exercise the same sample density as the primary row.
- `metrics.jsonl` contains both replay rows for each fixture; the report tables use replay 0 after requiring replay 1 to match hashes and post-state.

## Recommendation

Slice 5 replay, material accounting, and runtime gates pass. Pause for user review before selecting the next design slice.
