# Phase III Sub-Phase IIIC Consolidated Checkpoint

Artifacts root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIICConsolidation/20260503T043803Z`

Status: IIIC.1-IIIC.5 consolidated. This checkpoint closes the subduction-mutation sub-phase by proving the consolidated process-layer preset, the disabled regression path, and the slab-pull on/off differential. It does not add collision, rifting, erosion, terrain displacement, projection-derived ownership, or any new resampling mutation path.

Consolidated control shape: `ConfigurePhaseIIICProcessLayer(true, false)` enables subducting-triangle marks, visible/historical elevation split, and overriding-plate uplift. Slab pull remains a separate authority-feedback switch and stays off unless requested with `ConfigurePhaseIIICProcessLayer(true, true)`.

## Gate Summary

| Gate | Result | Evidence |
|---|---|---|
| Slice 5.5 bypass | pass | projection `a411b6aad7877a55` / `a411b6aad7877a55`, state `3b4a85366dab80db` / `3b4a85366dab80db`, crust `a4e4e99de216c31c` / `a4e4e99de216c31c`, material `bc3077100ba291b4` / `bc3077100ba291b4`, convergence `60a0688dd0b05b59` / `60a0688dd0b05b59`, no-boundary fallback 0/0 |
| IIIB independent signature gate | pass | computed `bf8818a26ed7b1dc` / `bf8818a26ed7b1dc`, expected `bf8818a26ed7b1dc`; closure `5445f4e8caa2c020` / `5445f4e8caa2c020` |
| Consolidated process layer, slab pull off | pass | marks 84 / 84, trench 149 / 149, uplift 11040 / 11040, rollup `97ccacbfb1abb524` |
| Disabled process layer | pass | marks 0, records 0, motion `96d2f3bac07b3cda` -> `96d2f3bac07b3cda` |
| Slab pull opt-in differential | pass | contributions 84 / 84, motion `96d2f3bac07b3cda` -> `7b00b66aaa1c9e83`, rollup `d911006702f6d4df` |
| Slab pull independent oracle | pass | axis 0.000000000000e+00, angular 0.000000000000e+00, contribution 0.000000000000e+00, max velocity 100.000000 mm/yr |
| Negative controls | pass | zero 0, single 0, divergence-no-subduction 0 ledger records |

## Primary Replays

| Fixture | Replay | Process layer | Slab pull | Marks | Trench records | Uplift records | Ledger records | Actual delta km | Ledger residual km | Motion before | Motion after | Rollup | Seconds |
|---|---:|---|---|---:|---:|---:|---:|---:|---:|---|---|---|---:|
| process_layer_default | 0 | on | off | 84 | 149 | 11040 | 11189 | -51.176588935012 | 1.854516540334e-12 | `96d2f3bac07b3cda` | `96d2f3bac07b3cda` | `97ccacbfb1abb524` | 0.097 |
| process_layer_default | 1 | on | off | 84 | 149 | 11040 | 11189 | -51.176588935012 | 1.854516540334e-12 | `96d2f3bac07b3cda` | `96d2f3bac07b3cda` | `97ccacbfb1abb524` | 0.089 |
| process_layer_disabled | 0 | off | off | 0 | 0 | 0 | 0 | 0.000000000000 | 0.000000000000e+00 | `96d2f3bac07b3cda` | `96d2f3bac07b3cda` | `f61f5ba6c0cc2a7d` | 0.099 |
| slab_pull_opt_in | 0 | on | on | 84 | 149 | 11040 | 11189 | -51.176588935012 | 1.854516540334e-12 | `96d2f3bac07b3cda` | `7b00b66aaa1c9e83` | `d911006702f6d4df` | 0.092 |
| slab_pull_opt_in | 1 | on | on | 84 | 149 | 11040 | 11189 | -51.176588935012 | 1.854516540334e-12 | `96d2f3bac07b3cda` | `7b00b66aaa1c9e83` | `d911006702f6d4df` | 0.099 |
| zero_motion | 0 | on | off | 0 | 0 | 0 | 0 | 0.000000000000 | 0.000000000000e+00 | `84ba588e474acf95` | `84ba588e474acf95` | `071e3ccbcffbd162` | 0.094 |
| single_plate | 0 | on | off | 0 | 0 | 0 | 0 | 0.000000000000 | 0.000000000000e+00 | `b42b5f5c6fc22793` | `b42b5f5c6fc22793` | `02ecf8776bcdf33c` | 0.070 |
| forced_divergence_no_subduction | 0 | on | off | 0 | 0 | 0 | 0 | 0.000000000000 | 0.000000000000e+00 | `aa96c1b974567fbe` | `aa96c1b974567fbe` | `34598c2256d9a24a` | 0.060 |

## IIIB Regression Signature

This is a replay of the IIIB hardening discriminator fixture inside the IIIC consolidation commandlet. It compares the computed IIIB independent signature directly to the accepted `bf8818a26ed7b1dc` token; closure-hash self-recomputation alone is not sufficient for this gate. This token supersedes the original IIIB checkpoint token because the pre-IIID hardening added the zero no-boundary-pair fallback metric to the Slice 5.5 component of the independent signature.

| Replay | Step | Pair sign | Accepted local positives | Rejected local non-positives | Matrix pairs | Decisions | Propagation seeds | Propagation added | Computed signature | Expected signature | Slice 5.5 component | Closure component |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|---|---|
| 0 | 51 | -0.075341390677 | 432 | 293 | 1 | 1 | 413 | 22 | `bf8818a26ed7b1dc` | `bf8818a26ed7b1dc` | `5d14e0433e3a1e96` | `ee128f05184d6b3c` |
| 1 | 51 | -0.075341390677 | 432 | 293 | 1 | 1 | 413 | 22 | `bf8818a26ed7b1dc` | `bf8818a26ed7b1dc` | `5d14e0433e3a1e96` | `ee128f05184d6b3c` |

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
