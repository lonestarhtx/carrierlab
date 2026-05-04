# Phase III Pre-IIIE Cost Driver Identification

Status: pass. This slice adds diagnostic cost-driver timing only. It does not optimize collision, remesh, projection, or carrier code paths.

Output root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/PreIIIECostDriverIdentification/20260504T005908Z`

## Paper Table 2 Baseline

The published Table 2 baseline is `0.190s/step` for total tectonic processes at `60k` samples / `40` plates. IIID.8 replay 0 measured `1151.130s` over `32` steps (`35.972813s/step`, `189.33x`). The target tracked here is `<= 10x` paper cost. Over-target rows are findings, not pass/fail blockers.

Paper rows not yet active in implementation: oceanic crust generation `0.580s/event`, plate rifting `0.230s/event`.

## Fixture

Tier: `Slice`. Samples: `10000`. Plates: `2`. Threshold step: `2`. Commandlet wall time: `48.571227s`. Tier target: `120.0s`. Policy-resolved multi-hit count: `0`.

Setup split: spawn `0.000306s`, initialize `0.047102s`, configure `0.013495s`, threshold search `1.706528s`, total `1.767464s`. Setup rows below are amortized across the `32`-step IIID.8 window to make the short-window setup penalty visible.

## Timing Table

| Row | Category | Measured | Normalized cost | Paper Table 2 baseline | Ratio | Target | Probes | Candidates | Mutations | Nested calls |
|---|---|---:|---:|---:|---:|---|---:|---:|---:|---|
| setup_spawn_actor_amortized | setup | 0.000306s | 0.000010s/step | 0.190s/step | 0.00x | within_target | 0 | 0 | 0 | D1=0 D2=0 D3=0 D4=0 D5=0 D6=0 D7p=0 D7a=0 |
| setup_initialize_carrier_amortized | setup | 0.047102s | 0.001472s/step | 0.190s/step | 0.01x | within_target | 0 | 0 | 0 | D1=0 D2=0 D3=0 D4=0 D5=0 D6=0 D7p=0 D7a=0 |
| setup_fixture_config_amortized | setup | 0.013495s | 0.000422s/step | 0.190s/step | 0.00x | within_target | 0 | 0 | 0 | D1=0 D2=0 D3=0 D4=0 D5=0 D6=0 D7p=0 D7a=0 |
| setup_threshold_search_amortized | setup | 1.706528s | 0.053329s/step | 0.190s/step | 0.28x | within_target | 0 | 0 | 0 | D1=0 D2=0 D3=0 D4=0 D5=0 D6=0 D7p=0 D7a=0 |
| setup_total_amortized | setup | 1.767464s | 0.055233s/step | 0.190s/step | 0.29x | within_target | 0 | 0 | 0 | D1=0 D2=0 D3=0 D4=0 D5=0 D6=0 D7p=0 D7a=0 |
| advance_motion_rotation | per_step | 0.000465s | 0.000465s/step | 0.190s/step | 0.00x | within_target | 2 | 10000 | 0 | D1=0 D2=0 D3=0 D4=0 D5=0 D6=0 D7p=0 D7a=0 |
| iiib2_distance_update | per_step | 0.000009s | 0.000009s/step | 0.190s/step | 0.00x | within_target | 362 | 452 | 0 | D1=0 D2=0 D3=0 D4=0 D5=0 D6=0 D7p=0 D7a=0 |
| iiib3_matrix_total | per_step | 0.002749s | 0.002749s/step | 0.190s/step | 0.01x | within_target | 1086 | 182 | 0 | D1=0 D2=0 D3=0 D4=0 D5=0 D6=0 D7p=0 D7a=0 |
| iiib3_matrix_bvh_rebuild | per_step | 0.002401s | 0.002401s/step | 0.190s/step | 0.01x | within_target | 2 | 182 | 0 | D1=0 D2=0 D3=0 D4=0 D5=0 D6=0 D7p=0 D7a=0 |
| iiib3_matrix_ray_queries | per_step | 0.000348s | 0.000348s/step | 0.190s/step | 0.00x | within_target | 1086 | 528 | 0 | D1=0 D2=0 D3=0 D4=0 D5=0 D6=0 D7p=0 D7a=0 |
| iiib4_polarity_decisions | per_step | 0.000027s | 0.000027s/step | 0.190s/step | 0.00x | within_target | 182 | 1 | 0 | D1=0 D2=0 D3=0 D4=0 D5=0 D6=0 D7p=0 D7a=0 |
| iiib6_neighbor_propagation | per_step | 0.003770s | 0.003770s/step | 0.190s/step | 0.02x | within_target | 90 | 90 | 0 | D1=0 D2=0 D3=0 D4=0 D5=0 D6=0 D7p=0 D7a=0 |
| iiic1_mark_update | per_step | 0.000011s | 0.000011s/step | 0.190s/step | 0.00x | within_target | 528 | 90 | 0 | D1=0 D2=0 D3=0 D4=0 D5=0 D6=0 D7p=0 D7a=0 |
| iiic3_uplift | per_step | 0.005087s | 0.005087s/step | 0.190s/step | 0.03x | within_target | 90 | 15105 | 0 | D1=0 D2=0 D3=0 D4=0 D5=0 D6=0 D7p=0 D7a=0 |
| iiic4_slab_pull | per_step | 0.000002s | 0.000002s/step | 0.190s/step | 0.00x | within_target | 90 | 0 | 0 | D1=0 D2=0 D3=0 D4=0 D5=0 D6=0 D7p=0 D7a=0 |
| iiic5_ledger | per_step | 0.000227s | 0.000227s/step | 0.190s/step | 0.00x | within_target | 15105 | 90 | 0 | D1=0 D2=0 D3=0 D4=0 D5=0 D6=0 D7p=0 D7a=0 |
| advance_process_total | per_step | 0.012347s | 0.012347s/step | 0.190s/step | 0.06x | within_target | 1086 | 15105 | 0 | D1=0 D2=0 D3=0 D4=0 D5=0 D6=0 D7p=0 D7a=0 |
| iiid1_terrane_detection | collision | 0.797820s | 0.797820s/step | 0.190s/step | 4.20x | within_target | 176 | 90 | 0 | D1=1 D2=0 D3=0 D4=0 D5=0 D6=0 D7p=0 D7a=0 |
| iiid2_collision_grouping | collision | 0.799750s | 0.799750s/step | 0.190s/step | 4.21x | within_target | 90 | 1 | 0 | D1=1 D2=1 D3=0 D4=0 D5=0 D6=0 D7p=0 D7a=0 |
| iiid3_destination_mass | collision | 2.753711s | 2.753711s/step | 0.190s/step | 14.49x | over_target | 1 | 1 | 0 | D1=2 D2=1 D3=1 D4=0 D5=0 D6=0 D7p=0 D7a=0 |
| iiid4_slab_break_plan | collision | 3.550164s | 3.550164s/step | 0.190s/step | 18.69x | over_target | 90 | 1 | 0 | D1=3 D2=1 D3=1 D4=1 D5=0 D6=0 D7p=0 D7a=0 |
| iiid5_suture_plan | collision | 3.552894s | 3.552894s/step | 0.190s/step | 18.70x | over_target | 1 | 1 | 0 | D1=3 D2=1 D3=1 D4=1 D5=1 D6=0 D7p=0 D7a=0 |
| iiid7_uplift_plan | collision_elevation | 7.890916s | 7.890916s/step | 0.190s/step | 41.53x | over_target | 0 | 10 | 0 | D1=7 D2=3 D3=2 D4=2 D5=1 D6=0 D7p=1 D7a=0 |
| iiid6_topology_mutation | collision | 7.127022s | 7.127022s/step | 0.190s/step | 37.51x | over_target | 1 | 1 | 1 | D1=6 D2=2 D3=2 D4=2 D5=1 D6=1 D7p=0 D7a=0 |
| iiid7_uplift_apply | collision_elevation | 15.030029s | 15.030029s/step | 0.190s/step | 79.11x | over_target | 1 | 10 | 1 | D1=13 D2=5 D3=4 D4=4 D5=2 D6=1 D7p=1 D7a=1 |
| total_measured_cost_driver_surface | total | 45.064643s | 45.064643s/step | 0.190s/step | 237.18x | over_target | 20069 | 41940 | 2 | D1=0 D2=0 D3=0 D4=0 D5=0 D6=0 D7p=0 D7a=0 |

## Findings

- `integrated_gap`: IIID.8 replay 0 remains `189.33x` the paper Table 2 total-process baseline. This slice did not rerun the integrated replay; it decomposes a Slice-tier fixture to identify likely drivers.
- `largest_measured_slice_row`: `iiid7_uplift_apply` at `15.030029s/step` (`79.11x`).
- `largest_per_step_process_row`: `advance_process_total` at `0.012347s/step` (`0.06x`). IIIB.3 is split into BVH rebuild and ray-query sub-rows to distinguish mesh rebuild cost from ray-test cost.
- `largest_collision_row`: `iiid6_topology_mutation` at `7.127022s/step` (`37.51x`). Nested call counts show whether later gates re-run earlier scans instead of consuming a staged plan.
- `largest_elevation_collision_row`: `iiid7_uplift_apply` at `15.030029s/step` (`79.11x`).
- `first_order_conclusion`: in this existing Slice-tier fixture, one-step IIIB/IIIC process work and setup amortization are small; the immediate measured driver is the IIID.6/IIID.7 apply path, especially repeated nested recomputation of earlier collision plans.
- `scale_limit`: this does not prove IIIB.3/BVH work is harmless at the 60k/40 integrated scale. It says the first optimization target should be validated against the heavy IIID apply rows before changing core tracking logic.
- `nested_recompute_shape`: D4 calls D1 and D3; D5 calls D4; D6 calls D4 and D5; D7 plan calls D2, D4, and D5; D7 apply calls D7 plan and D6. The call-count column makes that recomputation visible as diagnostic evidence.
- `scope_limit`: rows are Slice-tier diagnostics on existing fixtures, not a replacement for mandatory Integrated-tier sub-phase evidence.

## Next Remediation Candidates

Do not optimize in this slice. The next optimization/design pass should start from the rows above, with special attention to any over-target row that is both large and repeatedly nested. Plausible remediation surfaces include staged-plan reuse inside commandlets, per-step BVH refresh avoidance when topology has not changed, and separating audit-record construction from hot process loops. Any such change needs its own slice and gates.

## Scope Notes

This commandlet adds diagnostics only. It does not add carrier authority state, global sample ownership, repair, recovery, backfill, cache-as-authority, projection-derived state, remesh replacement, collision optimization, or IIIE behavior.
