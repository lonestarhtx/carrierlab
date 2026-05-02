# Phase II Slice 2 Checkpoint: Polarity And Triangle Labels

Artifacts root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseII/Slice2/target_20260502`

This checkpoint adds read-only polarity and plate-local triangle labels on top of Slice 1 contact records. It does not filter projection candidates, resample, mutate material, or change plate topology. Triangle labels are evidence records for Slice 3's future filter integration.

Policy: fixture polarity wins only inside explicit polarity fixtures; otherwise mixed material uses oceanic-under-continental. Same-material contacts remain ambiguous. Third-plate contacts are outside the Slice 2 two-plate polarity model and emit no `subducting` or `overriding` labels.

## Gate Summary

| Fixture | Samples | Plates | Steps | Label hash match | Label log byte match | No mutation | Sign gate | No-label gate | Polarity gate | Ambiguity gate | Third-plate gate | Traceability | Direct-hit bound | Verdict |
|---|---:|---:|---:|---|---|---|---|---|---|---|---|---|---|---|
| default_60k | 60000 | 40 | 40 | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass |
| zero_motion | 10000 | 40 | 40 | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass |
| single_plate | 10000 | 1 | 40 | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass |
| forced_divergence | 10000 | 2 | 0 | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass |
| mixed_material | 10000 | 2 | 40 | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass |
| polarity_under_1 | 10000 | 2 | 40 | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass |
| polarity_under_0 | 10000 | 2 | 40 | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass |
| all_continental | 10000 | 2 | 40 | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass |
| ocean_only | 10000 | 2 | 40 | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass |

## Label Metrics

| Fixture | Contacts | Labelable | Labels | Unique triangles | Subducting | Overriding | Ambiguous | Third-plate out-of-scope | From third-plate | Max labels/contact | Label seconds | Label hash |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| default_60k | 33275 | 5461 | 10922 | 10852 | 2665 | 2665 | 5592 | 23807 | 0 | 2 | 0.002862 | `d50791de900a2519` |
| zero_motion | 666 | 0 | 0 | 0 | 0 | 0 | 0 | 33 | 0 | 0 | 0.000001 | `9aef0400c5bbb63f` |
| single_plate | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0.000000 | `9aed9e00c5b955ed` |
| forced_divergence | 84 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0.000001 | `9a657a00c5457f29` |
| mixed_material | 4791 | 1984 | 3968 | 3888 | 1984 | 1984 | 0 | 0 | 0 | 2 | 0.000762 | `4080e7bad97b11cb` |
| polarity_under_1 | 4791 | 1984 | 3968 | 3888 | 1984 | 1984 | 0 | 0 | 0 | 2 | 0.000930 | `03c9216f94f99047` |
| polarity_under_0 | 4791 | 1984 | 3968 | 3888 | 1984 | 1984 | 0 | 0 | 0 | 2 | 0.000796 | `09cba517d960f6cf` |
| all_continental | 4791 | 1984 | 3968 | 3888 | 0 | 0 | 3968 | 0 | 0 | 2 | 0.000881 | `108f6c122b19912f` |
| ocean_only | 4791 | 1984 | 3968 | 3888 | 0 | 0 | 3968 | 0 | 0 | 2 | 0.000711 | `108f6c122b19912f` |

## Polarity Source Metrics

| Fixture | Fixture-specified contacts | Mixed-material contacts | Same-material ambiguous contacts | Unexpected subducting plate | Invalid trace labels | Area proxy |
|---|---:|---:|---:|---:|---:|---:|
| default_60k | 0 | 2665 | 2796 | 0 | 0 | 2.272837565117 |
| zero_motion | 0 | 0 | 0 | 0 | 0 | 0.000000000000 |
| single_plate | 0 | 0 | 0 | 0 | 0 | 0.000000000000 |
| forced_divergence | 0 | 0 | 0 | 0 | 0 | 0.000000000000 |
| mixed_material | 0 | 1984 | 0 | 0 | 0 | 4.885804894863 |
| polarity_under_1 | 1984 | 0 | 0 | 0 | 0 | 4.885804894863 |
| polarity_under_0 | 1984 | 0 | 0 | 0 | 0 | 4.885804894863 |
| all_continental | 0 | 0 | 1984 | 0 | 0 | 4.885804894863 |
| ocean_only | 0 | 0 | 1984 | 0 | 0 | 4.885804894863 |

## Directional Motion Probe

| Fixture | Signed convergence velocity replay A | Signed convergence velocity replay B | Interpretation |
|---|---:|---:|---|
| forced_divergence | -0.075341390677 | -0.075341390677 | separating |
| mixed_material | 0.075341390677 | 0.075341390677 | converging |
| polarity_under_1 | 0.075341390677 | 0.075341390677 | converging |
| polarity_under_0 | 0.075341390677 | 0.075341390677 | converging |
| all_continental | 0.075341390677 | 0.075341390677 | converging |
| ocean_only | 0.075341390677 | 0.075341390677 | converging |

## Notes

- `default_60k`: Third-plate contacts are counted as out-of-scope evidence and emit no subducting or overriding triangle labels.
- `zero_motion`: Control fixture should emit no triangle labels because it supplies no two-plate convergent labeling evidence.
- `single_plate`: Control fixture should emit no triangle labels because it supplies no two-plate convergent labeling evidence.
- `forced_divergence`: Control fixture should emit no triangle labels because it supplies no two-plate convergent labeling evidence.
- `polarity_under_1`: Fixture polarity pins under plate 1 and over plate 0; this is a Slice 2 oracle, not a production fallback.
- `polarity_under_0`: Fixture polarity pins under plate 0 and over plate 1; this is a Slice 2 oracle, not a production fallback.
- `all_continental`: Same-material convergent contacts emit ambiguous labels only; no subducting or overriding labels are invented.
- `ocean_only`: Same-material convergent contacts emit ambiguous labels only; no subducting or overriding labels are invented.
- The direct-hit bound means Slice 2 labels only the two plate-local triangles cited by a non-third-plate convergent contact. There is no propagation into a broad mask.
- `labels.jsonl` is the Slice 2 process evidence. Slice 3 may consume `subducting` labels through the resampling filter hook, but this checkpoint deliberately does not filter anything.

## Recommendation

Go for user review of Phase II Slice 2. The labeler is deterministic, non-mutating, traceable to Slice 1 contacts and plate-local triangles, and keeps third-plate evidence outside the two-plate polarity model. Do not advance to Slice 3 until the user records explicit go/no-go.
