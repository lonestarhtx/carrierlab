# Phase III Pre-IIIE Performance Containment

Status: pass. This checkpoint installs validation-tier policy and diagnostic timing before IIIE. It does not optimize collision, remesh, or projection code.

Output root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/PreIIIEPerformanceContainment/20260504T003556Z`

## Paper Table 2 Grounding

The paper reports total tectonic-process cost of `0.19s/step` at `60k` samples / `40` plates in Table 2 (page 9, `docs/ProceduralTectonicPlanets/ProceduralTectonicPlanets.pdf`). The current IIID.8 integrated replay 0 took `1151.130s` for `32` steps, or `35.972813s/step`, which is `189.33x` the paper total-process baseline. That is not a fact of nature to absorb with tiering; it is a tracked structural performance finding. A `<= 10x` paper-baseline ratio is the current soft target. Exceeding it flags a finding, not an implementation blocker.

Paper Table 2 also lists oceanic crust generation at `0.58s` and plate rifting at `0.23s`; those rows become active comparison baselines once IIIE/IIIF land.

## Validation Tiers

| Tier | Default use | Target | Replay policy |
|---|---|---:|---|
| Tiny | focused deterministic semantic fixtures | <15s | optional replay 1 unless claiming determinism |
| Slice | default per-slice validation | <120s | replay 1 opt-in for known-expensive or failed-replay-0 investigation paths |
| Integrated | default sub-phase consolidation | mandatory | replay 1 default-on for gates expected to pass; failed replay 0 may skip replay 1 only with yellow-flag/investigation record |
| Benchmark | optional scaling/repeat diagnosis | none | explicit by operator |

Sub-phase consolidation integrated runs are mandatory. Per-slice work defaults to Slice tier. Consolidation defaults to Integrated tier. No soft-skip at consolidation: if an integrated run is skipped, failed, or interrupted, the consolidation report must dispatch the yellow flag by running the integrated test, deferring to a named future slice with rationale, or closing it with explicit reasoning.

Long collision+remesh replays currently exceed the paper Table 2 baseline by about `189x` and are tiered to opt-in for routine slice work pending cost-driver investigation. They remain mandatory at sub-phase consolidation.

## Diagnostic Timing

Tier run: `Slice`, fixture `10000` samples / `2` plates, threshold step `2`, commandlet wall time `54.405696s`, tier target `120.0s`. Rows are inclusive API timings, not an exclusive callgraph profile.

| Row | Measured cost | Per-step cost | Paper Table 2 baseline | Ratio | Target | Probes | Candidates | Mutations |
|---|---:|---:|---:|---:|---|---:|---:|---:|
| collision_detection_iiid1 | 0.968463s | 0.484231s/step | 0.190s/step | 2.55x | within_target | 176 | 90 | 0 |
| collision_grouping_iiid2 | 0.988905s | 0.494453s/step | 0.190s/step | 2.60x | within_target | 90 | 1 | 0 |
| event_selection_iiid3 | 3.317972s | 1.658986s/step | 0.190s/step | 8.73x | within_target | 1 | 1 | 0 |
| slab_break_plan_iiid4 | 4.325314s | 2.162657s/step | 0.190s/step | 11.38x | over_target | 90 | 1 | 0 |
| suture_plan_iiid5 | 4.309900s | 2.154950s/step | 0.190s/step | 11.34x | over_target | 1 | 1 | 0 |
| uplift_plan_iiid7 | 9.558223s | 4.779112s/step | 0.190s/step | 25.15x | over_target | 0 | 10 | 0 |
| uplift_apply_iiid7 | 18.073660s | 9.036830s/step | 0.190s/step | 47.56x | over_target | 1 | 10 | 1 |
| topology_mutation_iiid6 | 8.611710s | 4.305855s/step | 0.190s/step | 22.66x | over_target | 1 | 1 | 1 |
| total_measured_collision_surface | 50.154147s | 25.077074s/step | 0.190s/step | 131.98x | over_target | 360 | 115 | 2 |

## Findings

- `paper_table2_ratio`: IIID.8 replay 0 is `189.33x` paper Table 2 total-process baseline at the same 60k/40 configuration.
- `slab_break_plan_iiid4`: `11.38x` exceeds the `<= 10x` soft target in this fixture.
- `suture_plan_iiid5`: `11.34x` exceeds the `<= 10x` soft target in this fixture.
- `uplift_plan_iiid7`: `25.15x` exceeds the `<= 10x` soft target in this fixture.
- `uplift_apply_iiid7`: `47.56x` exceeds the `<= 10x` soft target in this fixture.
- `topology_mutation_iiid6`: `22.66x` exceeds the `<= 10x` soft target in this fixture.
- `total_measured_collision_surface`: `131.98x` exceeds the `<= 10x` soft target in this fixture.
- `lab_policy_influence`: collision timing fixture did not require centroid/random multi-hit resolution.
- `iiid8_replay1`: replay 1 was intentionally skipped in the IIID consolidation run after replay 0 failed the quantitative gate. That skip is an investigation record, not a passing determinism claim.

## Containment Rules

- Future commandlets must report their validation tier.
- If a commandlet emits a stop-and-investigate note, that note becomes a yellow flag.
- A yellow flag blocks sub-phase consolidation until it is dispatched by an integrated run, a named future-slice deferral with rationale, or explicit closure reasoning.
- Replay 1 is context-aware: default-on for consolidation gates expected to pass; opt-in or skipped only when replay 0 already failed, the path is investigation-only, or the report records the yellow flag/deferral.
- Lab remesh / Stage 1.5 remesh remains labeled as lab-policy until IIIE replaces the primary path.

## Cost Deferral, Not Cost Solution

This containment defers integrated cost to consolidation moments while preserving integrated measurement at sub-phase boundaries. Per-step cost reduction is a pending investigation. Treating the 189x gap as inevitable would accept a structural performance defect on a project whose paper reports interactive rates for this configuration.

## Next Slice

Queue `Pre-IIIE.2 Cost Driver Identification` before behavior work beyond IIIE.1. Scope: run against existing fixtures with this diagnostic timing instrumentation, add no new simulation behavior, identify the dominant per-step cost source, and propose targeted remediation. Likely suspects to time explicitly: IIIB.3 matrix construction, IIIB.6 neighbor propagation expansion, full audit struct construction every step, per-step BVH rebuilds for all plates, and one-time setup costs amortized over only 32 IIID.8 steps.

## Scope Notes

This slice adds diagnostics and policy only. It does not add carrier authority state, global sample ownership, repair, recovery, backfill, cache-as-authority, projection-derived state, remesh replacement, or collision optimization.
