# Phase IIIE.6.10 Manual Cadence Unsupported Multi-Hit Diagnosis

**Verdict:** PASS / DIAGNOSTIC ONLY. Selection still holds fail-loud at editor-default continental material; this slice classifies the unsupported multi-hit bucket and does not resolve, coalesce, skip, or mutate those records.

## Scope

- IIIE.6.10 changes no remesh semantics. It adds a commandlet and report only.
- The diagnostic uses the live actor editor defaults that exposed the problem: `100000` samples, `40` plates, seed `42`, speed `66.6666666667 mm/yr`, and `ContinentalPlateFraction = 0.30`.
- Earlier IIIE.6.4/6.5/6.6 default commandlets forced `ContinentalPlateFraction = 0.0`, which made them all-ocean harnesses and did not cover the mixed-material editor path visible in the live actor.
- A held remesh remains honest: event count and hashes do not change on manual hold, and the mode string stays `phase_iii_e6_live_hold_unresolved_multi_hit_*`.

## Default Parity Gate

| Field | Value | Gate |
|---|---:|---|
| SampleCount | `100000` | editor-default live scale |
| PlateCount | `40` | editor-default live scale |
| Seed | `42` | editor-default seed |
| ContinentalPlateFraction | `0.30` | editor default; `0.0` is ocean-only negative/control coverage, not default |
| VelocityMmPerYear | `66.66666667` | observed-speed default used by the workbench |
| Phase III process layer | `on` | matches workbench default; slab pull remains off |
| IIIE resolvers | `coalescing/shared/nearest/distance-tie on` | audited IIIE.6 selection chain, not Stage 1.5 |
| Non-separating veto | `off` | IIIE.6.9 paper-literal zero-hit generation restored |

## Cadence Sweep

| Scenario | Result | Evidence |
|---|---|---|
| manual_step_1_selection | pass | auto `0`, apply `0`, step `1->1`, events `0->0`, next `32->32`, unresolved `464`, unsupported `464`, buckets cross/third/equal/mixed `2084/7/0/464`, nearest cross/third/tie/unsupported `2084/7/0/464`, dtie fallback `0`, process `0`, mode `selection_only`, hash `64497c82a0c2164b`, 1.99s |
| manual_step_8_selection | pass | auto `0`, apply `0`, step `8->8`, events `0->0`, next `32->32`, unresolved `3004`, unsupported `3004`, buckets cross/third/equal/mixed `15555/722/0/3004`, nearest cross/third/tie/unsupported `15552/722/0/3004`, dtie fallback `3`, process `0`, mode `selection_only`, hash `6cf9b3d7e1915158`, 8.94s |
| manual_step_15_selection | pass | auto `0`, apply `0`, step `15->15`, events `0->0`, next `32->32`, unresolved `4202`, unsupported `4202`, buckets cross/third/equal/mixed `20564/2891/0/4202`, nearest cross/third/tie/unsupported `20560/2890/0/4202`, dtie fallback `5`, process `0`, mode `selection_only`, hash `84cd88e84196813f`, 20.33s |
| manual_step_16_apply_probe | pass | auto `0`, apply `1`, step `16->16`, events `0->0`, next `32->32`, unresolved `4301`, unsupported `4301`, buckets cross/third/equal/mixed `20246/3213/0/4301`, nearest cross/third/tie/unsupported `20244/3212/0/4301`, dtie fallback `3`, process `0`, mode `phase_iii_e6_live_hold_unresolved_multi_hit_4301`, hash `b5029f31a76c1d47`, 22.17s |
| manual_step_17_selection | pass | auto `0`, apply `0`, step `17->17`, events `0->0`, next `32->32`, unresolved `4219`, unsupported `4219`, buckets cross/third/equal/mixed `19522/3656/0/4219`, nearest cross/third/tie/unsupported `19520/3655/0/4219`, dtie fallback `3`, process `0`, mode `selection_only`, hash `8bcadb3448d6b71d`, 22.89s |
| manual_step_20_selection | pass | auto `0`, apply `0`, step `20->20`, events `0->0`, next `32->32`, unresolved `4489`, unsupported `4489`, buckets cross/third/equal/mixed `17033/4358/0/4489`, nearest cross/third/tie/unsupported `17031/4357/0/4489`, dtie fallback `3`, process `0`, mode `selection_only`, hash `de170abfce9e079f`, 26.07s |
| manual_step_24_selection | pass | auto `0`, apply `0`, step `24->24`, events `0->0`, next `32->32`, unresolved `5142`, unsupported `5142`, buckets cross/third/equal/mixed `13588/5219/0/5142`, nearest cross/third/tie/unsupported `13587/5219/0/5142`, dtie fallback `1`, process `0`, mode `selection_only`, hash `42ffbebf2b9a7e91`, 29.24s |
| manual_step_32_apply_probe | pass | auto `0`, apply `1`, step `32->32`, events `0->0`, next `64->64`, unresolved `5480`, unsupported `5480`, buckets cross/third/equal/mixed `10624/7693/0/5480`, nearest cross/third/tie/unsupported `10621/7692/0/5480`, dtie fallback `4`, process `0`, mode `phase_iii_e6_live_hold_unresolved_multi_hit_5480`, hash `87f0efcac517a4db`, 34.66s |
| manual_step_32_replay_selection | pass | auto `0`, apply `0`, step `32->32`, events `0->0`, next `64->64`, unresolved `5480`, unsupported `5480`, buckets cross/third/equal/mixed `10624/7693/0/5480`, nearest cross/third/tie/unsupported `10621/7692/0/5480`, dtie fallback `4`, process `0`, mode `selection_only`, hash `cff6eb5541b547ef`, 34.20s |
| manual_step_32_after_step16_hold | pass | auto `0`, apply `1`, step `32->32`, events `0->0`, next `64->64`, unresolved `5480`, unsupported `5480`, buckets cross/third/equal/mixed `10624/7693/0/5480`, nearest cross/third/tie/unsupported `10621/7692/0/5480`, dtie fallback `4`, process `0`, mode `phase_iii_e6_live_hold_unresolved_multi_hit_5480`, hash `87f0efcac517a4db`, 35.73s |
| manual_step_33_apply_probe | pass | auto `0`, apply `1`, step `33->33`, events `0->0`, next `64->64`, unresolved `5698`, unsupported `5698`, buckets cross/third/equal/mixed `10640/7853/0/5698`, nearest cross/third/tie/unsupported `10638/7851/0/5698`, dtie fallback `4`, process `0`, mode `phase_iii_e6_live_hold_unresolved_multi_hit_5698`, hash `f482160b027ad76e`, 35.87s |
| manual_step_40_selection | pass | auto `0`, apply `0`, step `40->40`, events `0->0`, next `64->64`, unresolved `6292`, unsupported `6292`, buckets cross/third/equal/mixed `9517/9932/0/6292`, nearest cross/third/tie/unsupported `9516/9928/0/6292`, dtie fallback `5`, process `0`, mode `selection_only`, hash `ddf06a970ea7d873`, 38.48s |
| manual_step_48_selection | pass | auto `0`, apply `0`, step `48->48`, events `0->0`, next `64->64`, unresolved `5853`, unsupported `5853`, buckets cross/third/equal/mixed `8349/11247/0/5853`, nearest cross/third/tie/unsupported `8346/11242/0/5853`, dtie fallback `8`, process `0`, mode `selection_only`, hash `9a81e8f6f30287fb`, 41.31s |
| manual_step_60_selection | pass | auto `0`, apply `0`, step `60->60`, events `0->0`, next `64->64`, unresolved `8861`, unsupported `8861`, buckets cross/third/equal/mixed `7273/9436/0/8861`, nearest cross/third/tie/unsupported `7272/9433/0/8861`, dtie fallback `4`, process `0`, mode `selection_only`, hash `3ab545e98f5b9619`, 45.04s |
| auto_cadence_step_32_apply_probe | pass | auto `1`, apply `1`, step `31->32`, events `0->0`, next `32->32`, unresolved `5480`, unsupported `5480`, buckets cross/third/equal/mixed `10624/7693/0/5480`, nearest cross/third/tie/unsupported `10621/7692/0/5480`, dtie fallback `4`, process `0`, mode `phase_iii_e6_live_hold_unresolved_multi_hit_5480`, hash `9b2edd2199d58069`, 34.83s |

## Unsupported Shape Distribution

| Scenario | same source | shared edge | shared vertex | no shared vertices | field mismatch | three common vertex | three edge intruder | three no common | non-boundary/interior | invalid |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| manual_step_1_selection | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 464 | 0 |
| manual_step_8_selection | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 3004 | 0 |
| manual_step_15_selection | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 4202 | 0 |
| manual_step_16_apply_probe | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 4301 | 0 |
| manual_step_17_selection | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 4219 | 0 |
| manual_step_20_selection | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 4489 | 0 |
| manual_step_24_selection | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 5142 | 0 |
| manual_step_32_apply_probe | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 5480 | 0 |
| manual_step_32_replay_selection | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 5480 | 0 |
| manual_step_32_after_step16_hold | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 5480 | 0 |
| manual_step_33_apply_probe | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 5698 | 0 |
| manual_step_40_selection | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 6292 | 0 |
| manual_step_48_selection | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 5853 | 0 |
| manual_step_60_selection | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 8861 | 0 |
| auto_cadence_step_32_apply_probe | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 5480 | 0 |

## Process And Field Cross-Reference

| Scenario | process-marked | max ray residual km | max scalar residual | max elevation residual km | max unit-vector residual |
|---|---:|---:|---:|---:|---:|
| manual_step_1_selection | 0 | 0.0773106 | 1 | 32.8054 | 1 |
| manual_step_8_selection | 0 | 0.156122 | 1 | 366.319 | 1 |
| manual_step_15_selection | 0 | 0.164893 | 1 | 769.241 | 1.99832 |
| manual_step_16_apply_probe | 0 | 0.15191 | 1 | 835.943 | 1.7986 |
| manual_step_17_selection | 0 | 0.165545 | 1 | 867.825 | 1.99749 |
| manual_step_20_selection | 0 | 0.166702 | 1 | 1070.41 | 1.98026 |
| manual_step_24_selection | 0 | 0.163486 | 1 | 1233.93 | 1.99997 |
| manual_step_32_apply_probe | 0 | 0.162286 | 1 | 1124.63 | 1.99995 |
| manual_step_32_replay_selection | 0 | 0.162286 | 1 | 1124.63 | 1.99995 |
| manual_step_32_after_step16_hold | 0 | 0.162286 | 1 | 1124.63 | 1.99995 |
| manual_step_33_apply_probe | 0 | 0.171032 | 1 | 1081.07 | 1 |
| manual_step_40_selection | 0 | 0.162226 | 1 | 1354.62 | 1.99996 |
| manual_step_48_selection | 0 | 0.167762 | 1 | 1039.77 | 1 |
| manual_step_60_selection | 0 | 0.173548 | 1 | 1450.83 | 1.70304 |
| auto_cadence_step_32_apply_probe | 0 | 0.162286 | 1 | 1124.63 | 1.99995 |

## Interpretation

- Max unsupported count across the sweep: `8861`.
- Max mixed-material bucket count across the sweep: `8861`.
- Max process-marked unsupported count across the sweep: `0`.
- Because the harness now uses `ContinentalPlateFraction = 0.30`, IIIE.6.10 directly covers the live editor path from the screenshots rather than the older all-ocean diagnostic path.
- If `mixed_material` dominates, the next slice should decide whether mixed continental/oceanic post-motion multi-hits are paper-domain ownership, a named lab policy, or upstream process-state marking. IIIE.6.10 deliberately does not make that decision.

## Stop Conditions Preserved

- Stop if any unsupported record mutates topology, uses prior-owner fallback, uses projection-derived authority, or routes through Stage 1.5.
- Stop if a manual held remesh increments events or changes projection/state/crust hashes.
- Stop if process-state marks explain a substantial fraction of unsupported records; that would redirect the next slice to IIIB/IIIC/IIID marking rather than a remesh tie-break policy.

## Artifacts

- JSONL metrics: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIE610ManualCadenceUnsupportedMultiHit/phase-iii-slice-iiie6-10-manual-cadence-unsupported-multihit-diagnosis.jsonl`

## Replay Check

- Manual step-32 apply probe vs selection replay: `selection and diagnosis hashes match`.
