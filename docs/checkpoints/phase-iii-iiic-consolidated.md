# Phase III Sub-Phase IIIC Consolidated Checkpoint

Artifacts root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICConsolidation/20260502T231335Z`

Status: IIIC.1-IIIC.5 consolidated. This checkpoint closes the subduction-mutation sub-phase by proving the consolidated process-layer preset, the disabled regression path, and the slab-pull on/off differential. It does not add collision, rifting, erosion, terrain displacement, projection-derived ownership, or any new resampling mutation path.

Consolidated control shape: `ConfigurePhaseIIICProcessLayer(true, false)` enables subducting-triangle marks, visible/historical elevation split, and overriding-plate uplift. Slab pull remains a separate authority-feedback switch and stays off unless requested with `ConfigurePhaseIIICProcessLayer(true, true)`.

## Gate Summary

| Gate | Result | Evidence |
|---|---|---|
| Slice 5.5 bypass | pass | state `3b4a85366dab80db` / `3b4a85366dab80db`, material ledger `bc3077100ba291b4` / `bc3077100ba291b4` |
| IIIB independent signature gate | pass | expected regression token `4df40569f5e51e1a`; closure hash recomputation still matches `51f1c267444ff160` |
| Consolidated process layer, slab pull off | pass | marks 84 / 84, trench 149 / 149, uplift 11040 / 11040, rollup `16632a6e19b904c1` |
| Disabled process layer | pass | marks 0, records 0, motion `96d2f3bac07b3cda` -> `96d2f3bac07b3cda` |
| Slab pull opt-in differential | pass | contributions 84 / 84, motion `96d2f3bac07b3cda` -> `7b00b66aaa1c9e83`, rollup `5bad65de6b800c2e` |
| Slab pull independent oracle | pass | axis 0.000000000000e+00, angular 0.000000000000e+00, contribution 0.000000000000e+00, max velocity 100.000000 mm/yr |
| Negative controls | pass | zero 0, single 0, divergence-no-subduction 0 ledger records |

## Primary Replays

| Fixture | Replay | Process layer | Slab pull | Marks | Trench records | Uplift records | Ledger records | Actual delta km | Ledger residual km | Motion before | Motion after | Rollup | Seconds |
|---|---:|---|---|---:|---:|---:|---:|---:|---:|---|---|---|---:|
| process_layer_default | 0 | on | off | 84 | 149 | 11040 | 11189 | -51.176588935012 | 1.854516540334e-12 | `96d2f3bac07b3cda` | `96d2f3bac07b3cda` | `16632a6e19b904c1` | 0.083 |
| process_layer_default | 1 | on | off | 84 | 149 | 11040 | 11189 | -51.176588935012 | 1.854516540334e-12 | `96d2f3bac07b3cda` | `96d2f3bac07b3cda` | `16632a6e19b904c1` | 0.082 |
| process_layer_disabled | 0 | off | off | 0 | 0 | 0 | 0 | 0.000000000000 | 0.000000000000e+00 | `96d2f3bac07b3cda` | `96d2f3bac07b3cda` | `c0aeeeecd17f9120` | 0.077 |
| slab_pull_opt_in | 0 | on | on | 84 | 149 | 11040 | 11189 | -51.176588935012 | 1.854516540334e-12 | `96d2f3bac07b3cda` | `7b00b66aaa1c9e83` | `5bad65de6b800c2e` | 0.082 |
| slab_pull_opt_in | 1 | on | on | 84 | 149 | 11040 | 11189 | -51.176588935012 | 1.854516540334e-12 | `96d2f3bac07b3cda` | `7b00b66aaa1c9e83` | `5bad65de6b800c2e` | 0.083 |
| zero_motion | 0 | on | off | 0 | 0 | 0 | 0 | 0.000000000000 | 0.000000000000e+00 | `84ba588e474acf95` | `84ba588e474acf95` | `af7f80dc1b83676b` | 0.074 |
| single_plate | 0 | on | off | 0 | 0 | 0 | 0 | 0.000000000000 | 0.000000000000e+00 | `b42b5f5c6fc22793` | `b42b5f5c6fc22793` | `b1a98902dc424e8c` | 0.055 |
| forced_divergence_no_subduction | 0 | on | off | 0 | 0 | 0 | 0 | 0.000000000000 | 0.000000000000e+00 | `aa96c1b974567fbe` | `aa96c1b974567fbe` | `20206dbc25dda1f7` | 0.056 |

## Sub-Slice Closure

| Slice | Consolidated result |
|---|---|
| IIIC.1 marks | pass: consolidated process layer emits 84 deterministic subducting-triangle marks. |
| IIIC.2 elevation split | pass: trench line emits 149 records and visible/historical hashes are stable. |
| IIIC.3 uplift | pass: uplift line emits 11040 records with residual 4.547473508865e-13 km. |
| IIIC.4 slab pull | pass: opt-in path emits 84 contributions and changes motion hash without affecting the off path. |
| IIIC.5 elevation ledger | pass: full ledger has 11189 records and residual 1.854516540334e-12 km. |

## Scope Notes

- IIIC is now consolidated as a process-layer preset plus an independent slab-pull switch. The slice-level booleans remain for commandlet fixtures and narrow regression tests, but new Phase III code should prefer `ConfigurePhaseIIICProcessLayer` for the normal IIIC stack.
- Slab pull remains default-off and opt-in because it is the first authority-feedback loop. Turning it on is deterministic and oracle-checked here, but future paper-faithful long-horizon runs must still declare it explicitly.
- Stage 1.5 and Slice 5.5 open evidence remains preserved. IIIC explains subduction/elevation/slab-pull behavior only; it does not claim carrier success, collision resolution, rifting, divergent oceanic generation, erosion, or terrain morphology.
- The disabled process-layer replay is a fixed-fixture regression, not a proof that every possible run is unchanged.

Overall result: **pass**.

## Recommendation

IIIC consolidation passes. Phase IIID planning/implementation may begin after user review.
