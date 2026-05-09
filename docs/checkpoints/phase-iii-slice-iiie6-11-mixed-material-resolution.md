# Phase IIIE.6.11 Mixed-Material Nearest-Hit Resolution

**Verdict:** PASS. IIIE.6.11 amends the existing nearest-hit lab policy to include mixed-material strict unique-nearest records, while mixed-material distance ties flow through the existing IIIE.6.6 fallback hierarchy. Baseline rows keep the IIIE.6.10 hold reproducible with the extension disabled.

## Scope

- IIIE.6.11 changes remesh source selection semantics only for the existing nearest-hit lab policy's mixed-material bucket coverage.
- The diagnostic uses the live actor editor defaults that exposed the problem: `100000` samples, `40` plates, seed `42`, speed `66.6666666667 mm/yr`, and `ContinentalPlateFraction = 0.30`.
- Earlier IIIE.6.4/6.5/6.6 default commandlets forced `ContinentalPlateFraction = 0.0`, which made them all-ocean harnesses and did not cover the mixed-material editor path visible in the live actor.
- With `bExtendPhaseIIIE3NearestHitToMixedMaterial = false`, held remesh remains honest: event count and hashes do not change on manual hold, and the mode string stays `phase_iii_e6_live_hold_unresolved_multi_hit_*`.

- The default commandlet runs a cadence selection sweep. Passing `-ApplyProbeStep=N` adds one default-scale manual live-apply proof at that step; `-AutoApplyProbe` adds the natural-cadence apply proof; `-FullApplySweep` upgrades every swept manual row to live apply. Use the monitored runner for apply modes.

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
| Mixed-material nearest-hit extension | `on` for extension-enabled rows; `off` for baseline rows | preserves IIIE.6.10 evidence while closing live default path |
| Non-separating veto | `off` | IIIE.6.9 paper-literal zero-hit generation restored |

## Cadence Sweep

| Scenario | Result | Evidence |
|---|---|---|
| manual_step_1_selection | pass | auto `0`, apply `0`, extension_disabled `0`, step `1->1`, events `0->0`, next `32->32`, unresolved `0`, unsupported `0`, buckets cross/third/equal/mixed `2084/7/0/0`, nearest cross/third/mixed/tie/unsupported `2084/7/464/0/0`, dtie fallback total/mixed `0/0`, process `0`, mode `selection_only`, hash `2e4350fee18f64df`, 2.00s |
| manual_step_8_selection | pass | auto `0`, apply `0`, extension_disabled `0`, step `8->8`, events `0->0`, next `32->32`, unresolved `0`, unsupported `0`, buckets cross/third/equal/mixed `15555/722/0/0`, nearest cross/third/mixed/tie/unsupported `15552/722/3004/0/0`, dtie fallback total/mixed `3/0`, process `0`, mode `selection_only`, hash `a773401fa4e45efe`, 8.92s |
| manual_step_15_selection | pass | auto `0`, apply `0`, extension_disabled `0`, step `15->15`, events `0->0`, next `32->32`, unresolved `0`, unsupported `0`, buckets cross/third/equal/mixed `20564/2891/0/0`, nearest cross/third/mixed/tie/unsupported `20560/2890/4200/0/0`, dtie fallback total/mixed `7/2`, process `0`, mode `selection_only`, hash `288d710fc65e913d`, 20.34s |
| manual_step_16_selection | pass | auto `0`, apply `0`, extension_disabled `0`, step `16->16`, events `0->0`, next `32->32`, unresolved `0`, unsupported `0`, buckets cross/third/equal/mixed `20246/3213/0/0`, nearest cross/third/mixed/tie/unsupported `20244/3212/4301/0/0`, dtie fallback total/mixed `3/0`, process `0`, mode `selection_only`, hash `85fc0c90297de9b7`, 21.53s |
| manual_step_17_selection | pass | auto `0`, apply `0`, extension_disabled `0`, step `17->17`, events `0->0`, next `32->32`, unresolved `0`, unsupported `0`, buckets cross/third/equal/mixed `19522/3656/0/0`, nearest cross/third/mixed/tie/unsupported `19520/3655/4218/0/0`, dtie fallback total/mixed `4/1`, process `0`, mode `selection_only`, hash `16e7c463baa9e2a5`, 22.79s |
| manual_step_20_selection | pass | auto `0`, apply `0`, extension_disabled `0`, step `20->20`, events `0->0`, next `32->32`, unresolved `0`, unsupported `0`, buckets cross/third/equal/mixed `17033/4358/0/0`, nearest cross/third/mixed/tie/unsupported `17031/4357/4488/0/0`, dtie fallback total/mixed `4/1`, process `0`, mode `selection_only`, hash `ec7972cb09712f7e`, 26.02s |
| manual_step_24_selection | pass | auto `0`, apply `0`, extension_disabled `0`, step `24->24`, events `0->0`, next `32->32`, unresolved `0`, unsupported `0`, buckets cross/third/equal/mixed `13588/5219/0/0`, nearest cross/third/mixed/tie/unsupported `13587/5219/5141/0/0`, dtie fallback total/mixed `2/1`, process `0`, mode `selection_only`, hash `2c70ec8323a2931f`, 29.19s |
| manual_step_32_selection | pass | auto `0`, apply `0`, extension_disabled `0`, step `32->32`, events `0->0`, next `64->64`, unresolved `0`, unsupported `0`, buckets cross/third/equal/mixed `10624/7693/0/0`, nearest cross/third/mixed/tie/unsupported `10621/7692/5480/0/0`, dtie fallback total/mixed `4/0`, process `0`, mode `selection_only`, hash `13889733eb223a76`, 33.93s |
| manual_step_33_selection | pass | auto `0`, apply `0`, extension_disabled `0`, step `33->33`, events `0->0`, next `64->64`, unresolved `0`, unsupported `0`, buckets cross/third/equal/mixed `10640/7853/0/0`, nearest cross/third/mixed/tie/unsupported `10638/7851/5696/0/0`, dtie fallback total/mixed `6/2`, process `0`, mode `selection_only`, hash `895e25fbb9514e3f`, 34.38s |
| manual_step_40_selection | pass | auto `0`, apply `0`, extension_disabled `0`, step `40->40`, events `0->0`, next `64->64`, unresolved `0`, unsupported `0`, buckets cross/third/equal/mixed `9517/9932/0/0`, nearest cross/third/mixed/tie/unsupported `9516/9928/6292/0/0`, dtie fallback total/mixed `5/0`, process `0`, mode `selection_only`, hash `788c4b20affb713a`, 37.55s |
| manual_step_48_selection | pass | auto `0`, apply `0`, extension_disabled `0`, step `48->48`, events `0->0`, next `64->64`, unresolved `0`, unsupported `0`, buckets cross/third/equal/mixed `8349/11247/0/0`, nearest cross/third/mixed/tie/unsupported `8346/11242/5852/0/0`, dtie fallback total/mixed `9/1`, process `0`, mode `selection_only`, hash `7bd74d082ec9bd9c`, 40.54s |
| manual_step_60_selection | pass | auto `0`, apply `0`, extension_disabled `0`, step `60->60`, events `0->0`, next `64->64`, unresolved `0`, unsupported `0`, buckets cross/third/equal/mixed `7273/9436/0/0`, nearest cross/third/mixed/tie/unsupported `7272/9433/8860/0/0`, dtie fallback total/mixed `5/1`, process `0`, mode `selection_only`, hash `084ec8b1506488fc`, 44.10s |
| manual_step_32_replay_selection | pass | auto `0`, apply `0`, extension_disabled `0`, step `32->32`, events `0->0`, next `64->64`, unresolved `0`, unsupported `0`, buckets cross/third/equal/mixed `10624/7693/0/0`, nearest cross/third/mixed/tie/unsupported `10621/7692/5480/0/0`, dtie fallback total/mixed `4/0`, process `0`, mode `selection_only`, hash `13889733eb223a76`, 34.05s |
| baseline_step_16_mixed_material_extension_disabled_selection | pass | auto `0`, apply `0`, extension_disabled `1`, step `16->16`, events `0->0`, next `32->32`, unresolved `4301`, unsupported `4301`, buckets cross/third/equal/mixed `20246/3213/0/4301`, nearest cross/third/mixed/tie/unsupported `20244/3212/0/0/4301`, dtie fallback total/mixed `3/0`, process `0`, mode `selection_only`, hash `b880b4b374b4b550`, 21.97s |
| baseline_step_32_mixed_material_extension_disabled_selection | pass | auto `0`, apply `0`, extension_disabled `1`, step `32->32`, events `0->0`, next `64->64`, unresolved `5480`, unsupported `5480`, buckets cross/third/equal/mixed `10624/7693/0/5480`, nearest cross/third/mixed/tie/unsupported `10621/7692/0/0/5480`, dtie fallback total/mixed `4/0`, process `0`, mode `selection_only`, hash `a04395859f77cf38`, 34.14s |

## Unsupported Shape Distribution

| Scenario | same source | shared edge | shared vertex | no shared vertices | field mismatch | three common vertex | three edge intruder | three no common | non-boundary/interior | invalid |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| manual_step_1_selection | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| manual_step_8_selection | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| manual_step_15_selection | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| manual_step_16_selection | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| manual_step_17_selection | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| manual_step_20_selection | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| manual_step_24_selection | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| manual_step_32_selection | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| manual_step_33_selection | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| manual_step_40_selection | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| manual_step_48_selection | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| manual_step_60_selection | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| manual_step_32_replay_selection | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| baseline_step_16_mixed_material_extension_disabled_selection | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 4301 | 0 |
| baseline_step_32_mixed_material_extension_disabled_selection | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 5480 | 0 |

## Process And Field Cross-Reference

| Scenario | process-marked | max ray residual km | max scalar residual | max elevation residual km | max unit-vector residual |
|---|---:|---:|---:|---:|---:|
| manual_step_1_selection | 0 | 0 | 0 | 0 | 0 |
| manual_step_8_selection | 0 | 0 | 0 | 0 | 0 |
| manual_step_15_selection | 0 | 0 | 0 | 0 | 0 |
| manual_step_16_selection | 0 | 0 | 0 | 0 | 0 |
| manual_step_17_selection | 0 | 0 | 0 | 0 | 0 |
| manual_step_20_selection | 0 | 0 | 0 | 0 | 0 |
| manual_step_24_selection | 0 | 0 | 0 | 0 | 0 |
| manual_step_32_selection | 0 | 0 | 0 | 0 | 0 |
| manual_step_33_selection | 0 | 0 | 0 | 0 | 0 |
| manual_step_40_selection | 0 | 0 | 0 | 0 | 0 |
| manual_step_48_selection | 0 | 0 | 0 | 0 | 0 |
| manual_step_60_selection | 0 | 0 | 0 | 0 | 0 |
| manual_step_32_replay_selection | 0 | 0 | 0 | 0 | 0 |
| baseline_step_16_mixed_material_extension_disabled_selection | 0 | 0.15191 | 1 | 835.943 | 1.7986 |
| baseline_step_32_mixed_material_extension_disabled_selection | 0 | 0.162286 | 1 | 1124.63 | 1.99995 |

## Interpretation

- Max extension-enabled unsupported count across the sweep: `0`.
- Max extension-enabled held mixed-material bucket count across the sweep: `0`.
- Max extension-enabled mixed-material selection bucket count resolved by the amendment: `8861`.
- Max extension-enabled process-marked unsupported count across the sweep: `0`.
- Max extension-disabled baseline unsupported count: `5480`.
- Because the harness now uses `ContinentalPlateFraction = 0.30`, IIIE.6.11 directly covers the live editor path from the screenshots rather than the older all-ocean diagnostic path.
- The extension-enabled rows must show `unsupported == 0`, `unresolved == 0`, and live apply mode. The extension-disabled rows must reproduce the IIIE.6.10 mixed-material hold as historical evidence.
- Mixed-material nearest-hit remains an amendment to the existing nearest-hit lab policy, not a new paper-cited rule and not an eighth IIIE lab policy.

## Targeted Live Apply Proof

The full cadence sweep above is selection-only by default because the live apply path remains several minutes per cadence at `100000` samples. To verify the extension-enabled editor path actually mutates, a monitored commandlet run was executed with `-ApplyProbeStep=16 -OnlyApplyProbe`, matching the screenshot cadence state that previously held on `MixedMaterial -> UnsupportedHeld`.

| Scenario | Result | Evidence |
|---|---|---|
| manual_step_16_apply_probe_only | pass | elapsed `290.486s`, step `16->16`, events `0->1`, mode `phase_iii_e6_live_apply`, unresolved `0`, unsupported `0`, nearest_mixed `4301`, distance_tie_fallback `3`, generated `33493`, applied `24644`, rift_pending `8849`, invalid `0`, non_positive_separation_observed `16404`, majority `32307`, triple_junction_split `508` |

The live apply commandlet emitted progress logs every `10000` records and an explicit topology-rebuild marker, so the long run was monitored for real progress rather than allowed to sit silently.

## Stop Conditions Preserved

- Stop if any extension-enabled unsupported record remains after the amendment.
- Stop if the extension-disabled baseline mutates topology, uses prior-owner fallback, uses projection-derived authority, or routes through Stage 1.5.
- Stop if a baseline held remesh increments events or changes projection/state/crust hashes.
- Stop if process-state marks explain a substantial fraction of any remaining unsupported records; that would redirect to IIIB/IIIC/IIID marking rather than remesh tie-break policy.

## Artifacts

- JSONL metrics: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIE611MixedMaterialNearestHit/phase-iii-slice-iiie6-11-mixed-material-resolution.jsonl`

## Replay Check

- Manual step-32 primary row vs selection replay: `selection and diagnosis hashes match`.
