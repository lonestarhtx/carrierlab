# Phase III Slice IIID.6 Report - Detach + Suture Topology Mutation

Generated: `2026-05-03 18:22:48` UTC

## Scope

IIID.6 is the first IIID slice that mutates plate-local topology. It consumes the accepted IIID.4 Slab Break and IIID.5 Suture plans, applies exactly one collision event for the timestep, removes the terrane triangles from the source plate, adds the same terrane triangles to the destination plate, reinitializes boundary tracking, and invalidates convergence tracking so IIIB can recompute it next. It does not apply uplift, resample, change plate motion, displace terrain, or invoke the lab-policy remesh path.

Metrics: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIID6/20260503T182152Z/metrics.jsonl`

## Gate Summary

| Gate | Result | Evidence |
|---|---:|---|
| Slice 5.5 fixed-fixture regression | pass | state `3b4a85366dab80db` / `3b4a85366dab80db`, ledger `bc3077100ba291b4` / `bc3077100ba291b4` |
| IIIB independent signature regression | pass | replay A `bf8818a26ed7b1dc`, replay B `bf8818a26ed7b1dc`, expected `bf8818a26ed7b1dc` |
| Detach + suture mutation deterministic | pass | replay A `440c1c40ea0fe68c`, replay B `440c1c40ea0fe68c`, applied 1 / 1 |
| Source/destination continental area conserved | pass | source delta -0.003769911184, destination delta 0.003769911184, residual -0.000000000000 |
| Boundary tracking reinitialized and convergence tracking invalidated | pass | reset serial 0 -> 1, invalidated evidence 176, invalidated polarity 1 |
| One collision only and no uplift applied | pass | deferred valid plans 0, elevation residual 0.000000000000, historical residual 0.000000000000 |
| Pure-oceanic negative emits no topology mutation | pass | decisions 1, collision candidates 0, applied 0 |
| No lab multi-hit policy influence | pass | policy-resolved multi-hit counts 0 / 0 / 0 / 0 |

## Fixture Results

| Fixture | Replay | Step | Matrix hits | Collision candidates | Slab plans | Suture plans | Applied | Deferred | Reset | Removed tris | Added tris | Source delta | Dest delta | Residual | Policy multi-hits | Baseline stable | Mutation changed | Hash |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---|
| source_patch_detach_suture_mutation | 0 | 2 | 346 | 1 | 1 | 1 | 1 | 0 | 0 -> 1 | 4 | 4 | -0.003769911184 | 0.003769911184 | -0.000000000000 | 0 | no | yes | `f055dbddf5893479` |
| source_patch_detach_suture_mutation | 1 | 2 | 346 | 1 | 1 | 1 | 1 | 0 | 0 -> 1 | 4 | 4 | -0.003769911184 | 0.003769911184 | -0.000000000000 | 0 | no | yes | `f055dbddf5893479` |
| pure_oceanic_negative | 0 | 1 | 170 | 0 | 0 | 0 | 0 | 0 | 0 -> 0 | 0 | 0 | 0.000000000000 | 0.000000000000 | 0.000000000000 | 0 | yes | no | `eeec9460c7fe7045` |
| pure_oceanic_negative | 1 | 1 | 170 | 0 | 0 | 0 | 0 | 0 | 0 -> 0 | 0 | 0 | 0.000000000000 | 0.000000000000 | 0.000000000000 | 0 | yes | no | `eeec9460c7fe7045` |

## Representative Mutation

- Source-patch replay A: source 1 -> destination 0, removed 4 triangles, added 4 triangles, source delta -0.003769911184, destination delta 0.003769911184, residual -0.000000000000, reset 0 -> 1, mutation hash `fc0ce8f017a90dbb`

## Interpretation

The source-patch fixture creates a destination-side continental receiver patch, waits for the 300 km interpenetration gate, and then applies the first valid IIID.4/IIID.5 plan. The mutation gate requires a one-for-one triangle transfer, equal and opposite continental-area deltas, topology validity on both plates, boundary-tracking reinitialization, and convergence-tracking invalidation for recomputation. The changed state/crust/convergence hashes in the mutation fixture are expected because IIID.6 is a carrier-topology mutation slice.

The pure-oceanic negative keeps the same forced-convergence motion but removes continental collision eligibility. It may produce convergence evidence, but it must not produce collision candidates, slab-break plans, suture plans, event-count changes, reset-serial changes, or topology mutations.

## Verdict

PASS. IIID.6 detach + suture topology mutation is deterministic and mass-conserving for the exercised fixtures. Stop for review before IIID.7 collision uplift.
