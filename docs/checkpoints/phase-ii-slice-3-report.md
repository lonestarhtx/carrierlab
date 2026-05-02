# Phase II Slice 3 Checkpoint: Resampling Filter Integration

Artifacts root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseII/Slice3/target_20260502`

This checkpoint consumes only Slice 2 `subducting` triangle labels inside the thesis resampling filter hook. It does not consume ambiguous labels, third-plate evidence, or centroid/random policies in the primary path. Samples that remain multi-hit after filtering are reported as unresolved; samples whose candidates are all filtered are reported as filter-exhausted rather than gap-filled.

## Gate Summary

| Fixture | Samples | Plates | Steps | Contact replay | Label replay | Decision replay | Decision log | Post-state replay | Auth CAF | Third-plate isolation | No-filter control | Expected filter | Filtered plate | Mixed signal | Post-filter multi | Trace | Sign | Verdict |
|---|---:|---:|---:|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| cadence_60k_primary | 60000 | 40 | 32 | pass | pass | pass | pass | pass | fail | pass | pass | pass | pass | pass | pass | pass | pass | investigate |
| forced_convergence_under_1 | 10000 | 2 | 40 | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass |
| forced_convergence_under_0 | 10000 | 2 | 40 | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass |
| forced_divergence_step0 | 10000 | 2 | 0 | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass |
| same_pair_mixed_signal | 10000 | 2 | 40 | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass |
| zero_motion | 10000 | 40 | 32 | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass |
| single_plate | 10000 | 1 | 32 | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass |

## Filter Metrics

| Fixture | Contacts | Conv | Div | Third | Labels | Subducting labels | Ambiguous labels | Filtered candidates | Filtered samples | Raw multi | Post multi | Post non-boundary multi | Unresolved | Exhausted | Gaps | Auth CAF before | Auth CAF after | Proj CAF before | Proj CAF after | Max area delta % | Event s | Decision hash |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| cadence_60k_primary | 29472 | 5164 | 4494 | 19813 | 10328 | 2163 | 6002 | 2164 | 2164 | 14715 | 12551 | 12551 | 12551 | 0 | 21273 | 0.301050000000 | 0.273075416991 | 0.191342288749 | 0.273075416991 | 142.305130 | 0.547970 | `68428bf8efa6b3c6` |
| forced_convergence_under_1 | 4791 | 1984 | 2807 | 0 | 3968 | 1984 | 0 | 1984 | 1984 | 4791 | 2807 | 2807 | 2807 | 0 | 4796 | 1.000000000000 | 1.000000000000 | 0.520400000000 | 1.000000000000 | 0.860516 | 0.042723 | `9fe30821b83edf98` |
| forced_convergence_under_0 | 4791 | 1984 | 2807 | 0 | 3968 | 1984 | 0 | 1984 | 1984 | 4791 | 2807 | 2807 | 2807 | 0 | 4796 | 1.000000000000 | 1.000000000000 | 0.520400000000 | 1.000000000000 | 40.564339 | 0.042474 | `a0a5b7d3bc2d5484` |
| forced_divergence_step0 | 84 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 84 | 84 | 0 | 84 | 0 | 0 | 0.500300000000 | 0.500300000000 | 0.500300000000 | 0.500300000000 | 0.100060 | 0.043874 | `652b65f2d2c5cf3e` |
| same_pair_mixed_signal | 4796 | 3012 | 1784 | 0 | 6024 | 3012 | 0 | 3013 | 3013 | 4796 | 1783 | 1783 | 1783 | 0 | 4791 | 1.000000000000 | 1.000000000000 | 0.520900000000 | 1.000000000000 | 11.887132 | 0.044675 | `487c40c512d5e5e7` |
| zero_motion | 666 | 0 | 0 | 33 | 0 | 0 | 0 | 0 | 0 | 644 | 644 | 0 | 644 | 0 | 0 | 0.300300000000 | 0.300300000000 | 0.300300000000 | 0.300300000000 | 3.212851 | 0.044305 | `ccf3c1d47bb228a4` |
| single_plate | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0.000000000000 | 0.000000000000 | 0.000000000000 | 0.000000000000 | 0.000000 | 0.039770 | `324662b424f6dd62` |

## Directional Motion Probe

| Fixture | Signed convergence replay A | Signed convergence replay B | Interpretation |
|---|---:|---:|---|
| forced_convergence_under_1 | 0.075341390677 | 0.075341390677 | converging |
| forced_convergence_under_0 | 0.075341390677 | 0.075341390677 | converging |
| forced_divergence_step0 | -0.075341390677 | -0.075341390677 | separating |
| same_pair_mixed_signal | -0.075341390677 | -0.075341390677 | separating |

## Notes

- `cadence_60k_primary`: Residual unresolved overlap is expected to be third-plate or ambiguous process evidence, not centroid fallback.
- `forced_convergence_under_1`: Fixture polarity expects filtered/subducting plate 1.
- `forced_convergence_under_0`: Fixture polarity expects filtered/subducting plate 0.
- `forced_divergence_step0`: Control fixture should produce no subducting-triangle filter decisions.
- `same_pair_mixed_signal`: Same plate pair intentionally includes convergent and divergent evidence; only locally labeled convergent triangles can filter.
- `zero_motion`: Control fixture should produce no subducting-triangle filter decisions.
- `single_plate`: Control fixture should produce no subducting-triangle filter decisions.
- Slice 3's primary path does not call `ChooseNearestCandidatePlate` for post-filter multi-hit samples. Those samples are reported as unresolved instead of being silently policy-resolved.
- `filter_decisions.jsonl` is replay-sufficient evidence for every consumed subducting-triangle label and every unresolved/filter-exhausted sample.
- The cadence 60k Auth CAF gate remains failed, consistent with Stage 1.5's accepted foundation-characterization finding: q1/q2 gap material transfer changes authoritative CAF even when convergent filtering is explicit. Slice 3 improves the convergent filter surface; it does not resolve material accounting.
- 250k scaling remains deferred to Slice 5; this Slice 3 checkpoint is the cadence-faithful 60k integration gate plus focused controls.

## Recommendation

Pause before Slice 4. One or more Slice 3 gates require investigation before material accounting is layered on top.
