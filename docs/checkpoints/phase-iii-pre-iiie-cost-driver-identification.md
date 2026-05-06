# Phase III Pre-IIIE Collision Plan Reuse / Cost Driver Identification

Status: pass. This slice adds behavior-preserving IIID collision-plan reuse and regenerates the cost-driver timing report. It does not add remesh, projection-derived correction, global ownership, cache-as-authority, or new tectonic behavior.

Output root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/PreIIIECostDriverIdentification/20260506T024918Z`

## Paper Table 2 Baseline

The published Table 2 baseline is `0.190s/step` for total tectonic processes at `60k` samples / `40` plates. IIID.8 replay 0 measured `1151.130s` over `32` steps (`35.972813s/step`, `189.33x`). The target tracked here is `<= 10x` paper cost. Over-target rows are findings, not pass/fail blockers.

Paper rows not yet active in implementation: oceanic crust generation `0.580s/event`, plate rifting `0.230s/event`.

## Fixture

Tier: `Slice`. Samples: `10000`. Plates: `2`. Threshold step: `2`. Commandlet wall time: `19.989339s`. Tier target: `120.0s`. Policy-resolved multi-hit count: `0`.

Setup split: spawn `0.000304s`, initialize `0.047866s`, configure `0.013163s`, threshold search `1.669572s`, total `1.730941s`. Setup rows below are amortized across the `32`-step IIID.8 window to make the short-window setup penalty visible.

## Timing Table

| Row | Category | Measured | Normalized cost | Paper Table 2 baseline | Ratio | Target | Probes | Candidates | Mutations | Nested calls |
|---|---|---:|---:|---:|---:|---|---:|---:|---:|---|
| setup_spawn_actor_amortized | setup | 0.000304s | 0.000010s/step | 0.190s/step | 0.00x | within_target | 0 | 0 | 0 | D1=0 D2=0 D3=0 D4=0 D5=0 D6=0 D7p=0 D7a=0 |
| setup_initialize_carrier_amortized | setup | 0.047866s | 0.001496s/step | 0.190s/step | 0.01x | within_target | 0 | 0 | 0 | D1=0 D2=0 D3=0 D4=0 D5=0 D6=0 D7p=0 D7a=0 |
| setup_fixture_config_amortized | setup | 0.013163s | 0.000411s/step | 0.190s/step | 0.00x | within_target | 0 | 0 | 0 | D1=0 D2=0 D3=0 D4=0 D5=0 D6=0 D7p=0 D7a=0 |
| setup_threshold_search_amortized | setup | 1.669572s | 0.052174s/step | 0.190s/step | 0.27x | within_target | 0 | 0 | 0 | D1=0 D2=0 D3=0 D4=0 D5=0 D6=0 D7p=0 D7a=0 |
| setup_total_amortized | setup | 1.730941s | 0.054092s/step | 0.190s/step | 0.28x | within_target | 0 | 0 | 0 | D1=0 D2=0 D3=0 D4=0 D5=0 D6=0 D7p=0 D7a=0 |
| advance_motion_rotation | per_step | 0.000410s | 0.000410s/step | 0.190s/step | 0.00x | within_target | 2 | 10000 | 0 | D1=0 D2=0 D3=0 D4=0 D5=0 D6=0 D7p=0 D7a=0 |
| iiib2_distance_update | per_step | 0.000012s | 0.000012s/step | 0.190s/step | 0.00x | within_target | 362 | 452 | 0 | D1=0 D2=0 D3=0 D4=0 D5=0 D6=0 D7p=0 D7a=0 |
| iiib3_matrix_total | per_step | 0.002873s | 0.002873s/step | 0.190s/step | 0.02x | within_target | 1086 | 182 | 0 | D1=0 D2=0 D3=0 D4=0 D5=0 D6=0 D7p=0 D7a=0 |
| iiib3_matrix_bvh_rebuild | per_step | 0.002485s | 0.002485s/step | 0.190s/step | 0.01x | within_target | 2 | 182 | 0 | D1=0 D2=0 D3=0 D4=0 D5=0 D6=0 D7p=0 D7a=0 |
| iiib3_matrix_ray_queries | per_step | 0.000388s | 0.000388s/step | 0.190s/step | 0.00x | within_target | 1086 | 528 | 0 | D1=0 D2=0 D3=0 D4=0 D5=0 D6=0 D7p=0 D7a=0 |
| iiib4_polarity_decisions | per_step | 0.000068s | 0.000068s/step | 0.190s/step | 0.00x | within_target | 182 | 1 | 0 | D1=0 D2=0 D3=0 D4=0 D5=0 D6=0 D7p=0 D7a=0 |
| iiib6_neighbor_propagation | per_step | 0.004087s | 0.004087s/step | 0.190s/step | 0.02x | within_target | 90 | 90 | 0 | D1=0 D2=0 D3=0 D4=0 D5=0 D6=0 D7p=0 D7a=0 |
| iiic1_mark_update | per_step | 0.000012s | 0.000012s/step | 0.190s/step | 0.00x | within_target | 528 | 90 | 0 | D1=0 D2=0 D3=0 D4=0 D5=0 D6=0 D7p=0 D7a=0 |
| iiic3_uplift | per_step | 0.005399s | 0.005399s/step | 0.190s/step | 0.03x | within_target | 90 | 15105 | 0 | D1=0 D2=0 D3=0 D4=0 D5=0 D6=0 D7p=0 D7a=0 |
| iiic4_slab_pull | per_step | 0.000001s | 0.000001s/step | 0.190s/step | 0.00x | within_target | 90 | 0 | 0 | D1=0 D2=0 D3=0 D4=0 D5=0 D6=0 D7p=0 D7a=0 |
| iiic5_ledger | per_step | 0.000237s | 0.000237s/step | 0.190s/step | 0.00x | within_target | 15105 | 90 | 0 | D1=0 D2=0 D3=0 D4=0 D5=0 D6=0 D7p=0 D7a=0 |
| advance_process_total | per_step | 0.013100s | 0.013100s/step | 0.190s/step | 0.07x | within_target | 1086 | 15105 | 0 | D1=0 D2=0 D3=0 D4=0 D5=0 D6=0 D7p=0 D7a=0 |
| iiid1_terrane_detection | collision | 0.792907s | 0.792907s/step | 0.190s/step | 4.17x | within_target | 176 | 90 | 0 | D1=1 D2=0 D3=0 D4=0 D5=0 D6=0 D7p=0 D7a=0 |
| iiid2_collision_grouping | collision | 0.786524s | 0.786524s/step | 0.190s/step | 4.14x | within_target | 90 | 1 | 0 | D1=1 D2=1 D3=0 D4=0 D5=0 D6=0 D7p=0 D7a=0 |
| iiid3_destination_mass | collision | 1.900975s | 1.900975s/step | 0.190s/step | 10.01x | over_target | 1 | 1 | 0 | D1=1 D2=0 D3=1 D4=0 D5=0 D6=0 D7p=0 D7a=0 |
| iiid4_slab_break_plan | collision | 1.885767s | 1.885767s/step | 0.190s/step | 9.93x | within_target | 90 | 1 | 0 | D1=1 D2=0 D3=0 D4=1 D5=0 D6=0 D7p=0 D7a=0 |
| iiid5_suture_plan | collision | 1.886104s | 1.886104s/step | 0.190s/step | 9.93x | within_target | 1 | 1 | 0 | D1=1 D2=0 D3=0 D4=0 D5=1 D6=0 D7p=0 D7a=0 |
| iiid7_uplift_plan | collision_elevation | 1.887070s | 1.887070s/step | 0.190s/step | 9.93x | within_target | 0 | 10 | 0 | D1=1 D2=0 D3=0 D4=0 D5=0 D6=0 D7p=1 D7a=0 |
| iiid6_topology_mutation | collision | 1.924870s | 1.924870s/step | 0.190s/step | 10.13x | over_target | 1 | 1 | 1 | D1=1 D2=0 D3=0 D4=0 D5=0 D6=1 D7p=0 D7a=0 |
| iiid7_uplift_apply | collision_elevation | 1.975446s | 1.975446s/step | 0.190s/step | 10.40x | over_target | 1 | 10 | 1 | D1=1 D2=0 D3=0 D4=0 D5=0 D6=1 D7p=0 D7a=1 |
| total_measured_cost_driver_surface | total | 16.530583s | 16.530583s/step | 0.190s/step | 87.00x | over_target | 20069 | 41940 | 2 | D1=0 D2=0 D3=0 D4=0 D5=0 D6=0 D7p=0 D7a=0 |

## Findings

- `integrated_gap`: IIID.8 replay 0 remains `189.33x` the paper Table 2 total-process baseline. This slice did not rerun the integrated replay; it decomposes a Slice-tier fixture to identify likely drivers.
- `plan_reuse_d6_delta`: `iiid6_topology_mutation` changed from `7.127022s/step` to `1.924870s/step` (`72.99%` reduction) by consuming the staged D1-D5 plan chain once instead of recomputing D4/D5 inside the mutation path.
- `plan_reuse_d7_delta`: `iiid7_uplift_apply` changed from `15.030029s/step` to `1.975446s/step` (`86.86%` reduction). The nested public-call shape collapsed from repeated D1-D5 rediscovery to one D1 seed plus one D6 mutation call.
- `plan_reuse_total_delta`: `total_measured_cost_driver_surface` changed from `45.064643s/step` to `16.530583s/step` (`63.32%` reduction). This total is still an inclusive diagnostic surface, not an exclusive profiler trace.
- `largest_measured_slice_row`: `iiid7_uplift_apply` at `1.975446s/step` (`10.40x`).
- `largest_per_step_process_row`: `advance_process_total` at `0.013100s/step` (`0.07x`). IIIB.3 is split into BVH rebuild and ray-query sub-rows to distinguish mesh rebuild cost from ray-test cost.
- `largest_collision_row`: `iiid6_topology_mutation` at `1.924870s/step` (`10.13x`). Nested call counts show whether later gates re-run earlier scans instead of consuming a staged plan.
- `largest_elevation_collision_row`: `iiid7_uplift_apply` at `1.975446s/step` (`10.40x`).
- `first_order_conclusion`: plan reuse removed the largest recomputation pattern. The remaining Slice-tier over-target rows are now the first-pass D1/DestinationMass-family scans themselves rather than repeated nested rediscovery in D6/D7 apply.
- `scale_limit`: this does not establish that IIIB.3/BVH work is harmless at the 60k/40 integrated scale. It says the first optimization target should be validated against the heavy IIID apply rows before changing core tracking logic.
- `nested_recompute_shape`: D6/D7 no longer call the earlier public stage APIs repeatedly. The call-count column remains diagnostic evidence for whether future edits accidentally reintroduce nested public-stage recomputation.
- `scope_limit`: rows are Slice-tier diagnostics on existing fixtures, not a replacement for mandatory Integrated-tier sub-phase evidence.

## Next Remediation Candidates

The next optimization/design pass should start from the remaining over-target rows, especially D1 terrane expansion and destination-mass/component construction. Per-step BVH refresh avoidance and audit-record construction are still possible future surfaces, but this run no longer points at D6/D7 nested recomputation as the dominant Slice-tier driver.

## Scope Notes

The actor change is plan reuse only: already-computed IIID audits are passed forward inside a single call chain. It does not add carrier authority state, global sample ownership, forbidden fallback-family behavior, cache-as-authority, projection-derived state, remesh replacement, or IIIE behavior.
