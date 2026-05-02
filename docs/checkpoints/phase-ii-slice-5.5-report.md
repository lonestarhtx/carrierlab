# Phase II Slice 5.5 Checkpoint: Single-Hit Ledger Subdivision

Artifacts root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseII/Slice55/verify_60k_20260502`

This checkpoint runs a targeted 60k validation of the accepted Slice 3+4 contact-label-filter-material-accounting stack with one additional read-only ledger subdivision: each `single_hit_transfer` record now records the hit triangle that supplied interpolated material and classifies that hit triangle as uniform continental, uniform oceanic, mixed, or unknown.

This does not add new process behavior. The purpose is to determine whether the Slice 5 single-hit material delta is coming from mixed hit triangles, which would point directly at boundary interpolation/dispersion, or from uniform hit triangles, which points at larger process-state gaps outside the current Phase II carrier foundation.

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

## Targeted 60k Metrics

| Resolution | Avg step kernel s | Avg step wall s | Paper Table 2 s | Contact s | Label s | Filter event s | Total replay s | Auth CAF before | Auth CAF after | Projected CAF before | Projected CAF after | Runtime |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| 60000 | 0.038126 | 0.048005 | 0.19 | 0.200612 | 0.004022 | 0.933084 | 3.052363 | 0.301050000000 | 0.273075416991 | 0.191342288749 | 0.273075416991 | pass |

## Material Delta Breakdown

| Resolution | Net C delta | Single-hit loss/gain/net | Subduction loss/gain/net | Gap-fill loss/gain/net | Unresolved same loss/gain/net | Unresolved triple loss/gain/net | Unresolved mixed loss/gain/net | Gap-fill net % of net loss | Single-hit net % of net loss |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 60000 | -0.351538977863 | 0.529486 / 0.154508 / -0.374977 | 0.000656 / 0.177194 / 0.176538 | 0.235777 / 0.082676 / -0.153100 | 0.000000 / 0.000000 / 0.000000 | 0.000000 / 0.000000 / 0.000000 | 0.000000 / 0.000000 / 0.000000 | 43.55% | 106.67% |

## Slice 5.5 Single-Hit Source Triangle Subdivision

This subdivision classifies the hit triangle used by each `single_hit_transfer` record. Uniform rows mean all three hit-triangle vertices share the same simplified material class; mixed rows are the only current evidence for interpolation across a carried material boundary.

| Resolution | Uniform continental count/net | Uniform oceanic count/net | Mixed triangle count/net | Unknown count/net |
|---:|---:|---:|---:|---:|
| 60000 | 7057 / 0.148074 | 16742 / -0.518782 | 168 / -0.004269 | 0 / 0.000000 |

## Count Breakdown

| Resolution | Contacts | Third-plate contacts | Labels | Material records | Material changed | Plate changed | Subduction | Gap fill | Non-sep gap | Unresolved same | Unresolved triple | Unresolved mixed | C residual | Max plate residual | Ledger hash |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| 60000 | 29472 | 19813 | 10328 | 59955 | 6904 | 45586 | 2164 | 21273 | 10437 | 6478 | 5056 | 1017 | 3.827e-13 | 6.661e-15 | `bc3077100ba291b4` |

## Notes

- Runtime `finding` means the average per-step projection kernel exceeded the paper Table 2 total for that resolution. It is reported as a scaling finding, not hidden by the gate labels.
- Gross loss/gain/net are reported together. Category priority should be based on net contribution and mechanism, not gross loss alone.
- Slice 5.5 provenance is observational only: hit-triangle uniformity is recorded after the resolver chooses a source triangle and does not influence resampling, filtering, labels, or carrier state.
- Material reconciliation uses `max(1e-12, total_area * 1e-12)` as the scaling gate. The residuals remain printed so floating-point summation noise cannot hide a real leak.
- Control fixtures are scaled with the target resolution in this commandlet so replay, label, filter, material, and no-material-change gates exercise the same sample density as the primary row.
- `metrics.jsonl` contains both replay rows for each fixture; the report tables use replay 0 after requiring replay 1 to match hashes and post-state.

## Finding

At 60k, the single-hit net continental delta is dominated by uniform source triangles, not mixed source triangles:

- Uniform oceanic hit triangles: `16,742` records, `-0.518782` net continental delta.
- Uniform continental hit triangles: `7,057` records, `+0.148074` net continental delta.
- Mixed hit triangles: `168` records, `-0.004269` net continental delta.
- Unknown hit triangles: `0` records.

So the single-hit loss is not primarily a barycentric interpolation smear across mixed material triangles. It is mostly coherent transfer into uniformly oceanic hit triangles, partly offset by coherent transfer into uniformly continental hit triangles. That makes this a process-state/accounting question for the next paper-reproduction phase, not evidence that Phase II's carrier foundation is numerically dispersing continents through mixed triangle interpolation.

## Recommendation

Slice 5.5 passes as a Phase II closeout hardening check. No Phase III behavior should be inferred from it beyond the handoff question it identifies: future crust/process state must decide when single-hit transfers into uniform oceanic triangles represent legitimate oceanic creation, subduction/collision state, or missing continental preservation logic.
