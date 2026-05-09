# Phase IIIE.6.4 Live Cadence + Post-Motion Multi-Hit Diagnosis

**Verdict:** PASS / DIAGNOSTIC ONLY. The live remesh blocker is still held fail-loud; this slice records why manual and automatic remesh attempts hold after motion and does not resolve cross-plate or third-plate cases.

## Scope

IIIE.6.4 adds cadence telemetry and post-motion multi-hit diagnosis only. It does not assign `cross_plate_different` or `third_plate` samples, does not use nearest-hit as authority, does not increment remesh event count on hold, and does not promote Stage 1.5 policy. The spatial PNG is diagnostic evidence only, not visual approval.

## Cadence Gates

| Gate | Result | Evidence |
|---|---|---|
| Default observed-speed cadence | pass | speed `66.6667`, observed-window `66.6667`, deltaT `64 Ma`, cadence `32`, next `32` |
| Zero-speed maximum cadence | pass | speed `0`, observed-window `0`, deltaT `128 Ma`, cadence `64`, next `64` |
| Fast-speed minimum cadence | pass | speed `150`, observed-window `150`, deltaT `32 Ma`, cadence `16`, next `16` |

Manual rows compare before/after a same-step remesh click. The automatic row spans the live step advance from 31 to 32, so projection/state/crust hash changes there are motion-step changes; the remesh-specific invariant is `events 0->0`, fail-loud mode, and overdue target preservation `next 32->32`.

## Manual And Automatic Remesh Diagnostics

| Scenario | Result | Evidence |
|---|---|---|
| manual_step_10 | pass | auto `0`, step `10->10`, events `0->0`, next `32->32`, speed/window `66.6667/66.6667`, cadence `32 steps / 64 Ma`, holds `24277`, crossDiff `22699`, third `1578`, process-marked `0`, unique-nearest crossDiff `22697`, distance-tie crossDiff `2`, nearest more-cont/older/lowerId `0/0/11645`, hashes `182d62768db7c39d/c6e5f90b815805f2/392976b507028224 -> 182d62768db7c39d/c6e5f90b815805f2/392976b507028224`, mode `phase_iii_e6_live_hold_unresolved_multi_hit_24277` |
| manual_step_20 | pass | auto `0`, step `20->20`, events `0->0`, next `32->32`, speed/window `66.6667/66.6667`, cadence `32 steps / 64 Ma`, holds `27038`, crossDiff `22189`, third `4849`, process-marked `0`, unique-nearest crossDiff `22186`, distance-tie crossDiff `3`, nearest more-cont/older/lowerId `0/0/11944`, hashes `7494ba96b24c42e6/9da595fa3542fed2/49c61d9444220b16 -> 7494ba96b24c42e6/9da595fa3542fed2/49c61d9444220b16`, mode `phase_iii_e6_live_hold_unresolved_multi_hit_27038` |
| manual_step_32 | pass | auto `0`, step `32->32`, events `0->0`, next `64->64`, speed/window `66.6667/66.6667`, cadence `32 steps / 64 Ma`, holds `24600`, crossDiff `16144`, third `8456`, process-marked `0`, unique-nearest crossDiff `16141`, distance-tie crossDiff `3`, nearest more-cont/older/lowerId `0/0/10378`, hashes `0be461d212b1a54a/ad21171e53f48d79/83de07ebd0edbf2a -> 0be461d212b1a54a/ad21171e53f48d79/83de07ebd0edbf2a`, mode `phase_iii_e6_live_hold_unresolved_multi_hit_24600` |
| manual_step_60 | pass | auto `0`, step `60->60`, events `0->0`, next `64->64`, speed/window `66.6667/66.6667`, cadence `32 steps / 64 Ma`, holds `26451`, crossDiff `16280`, third `10171`, process-marked `0`, unique-nearest crossDiff `16278`, distance-tie crossDiff `2`, nearest more-cont/older/lowerId `0/0/11640`, hashes `705a7827cadbd691/4c7bdbfd6a6469cd/3db874db54013fce -> 705a7827cadbd691/4c7bdbfd6a6469cd/3db874db54013fce`, mode `phase_iii_e6_live_hold_unresolved_multi_hit_26451` |
| auto_cadence_step_32 | pass | auto `1`, step `31->32`, events `0->0`, next `32->32`, speed/window `66.6667/66.6667`, cadence `32 steps / 64 Ma`, holds `24600`, crossDiff `16144`, third `8456`, process-marked `0`, unique-nearest crossDiff `16141`, distance-tie crossDiff `3`, nearest more-cont/older/lowerId `0/0/10378`, hashes `113a31bd102e0eb9/db7893beb0c6626a/1d33b3248c817839 -> 0be461d212b1a54a/ad21171e53f48d79/83de07ebd0edbf2a`, mode `phase_iii_e6_live_hold_unresolved_multi_hit_24600` |

## Nearest-Hit Diagnostic

Nearest-hit is computed as a parallel diagnostic for both `cross_plate_different` and `third_plate` holds. It is not a remesh source-selection rule for this commandlet, which runs with `bEnablePhaseIIIE3NearestHitTieBreak = false` so the historical 24,600-hold distribution is preserved as the diagnostic baseline. A unique nearest requires the nearest and second-nearest ray distances to differ by more than `1e-9 km`; ties remain ties.

| Scenario | Unique nearest crossDiff | Distance tie crossDiff | Unique nearest thirdPlate | Distance tie thirdPlate | More continental | Older oceanic | Lower plate id | Median gap km | P95 gap km |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| manual_step_10 | 22697 | 2 | 1578 | 0 | 0 | 0 | 11645 | 4.35571e-06 | 1.51563e-05 |
| manual_step_20 | 22186 | 3 | 4848 | 1 | 0 | 0 | 11944 | 4.41178e-06 | 1.52905e-05 |
| manual_step_32 | 16141 | 3 | 8455 | 1 | 0 | 0 | 10378 | 4.07054e-06 | 1.45231e-05 |
| manual_step_60 | 16278 | 2 | 10168 | 3 | 0 | 0 | 11640 | 4.07439e-06 | 1.43079e-05 |
| auto_cadence_step_32 | 16141 | 3 | 8455 | 1 | 0 | 0 | 10378 | 4.07054e-06 | 1.45231e-05 |

## Artifacts

- JSONL metrics: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIE64LiveCadencePostMotionMultiHit/phase-iii-slice-iiie6-4-live-cadence-post-motion-multihit-diagnosis-metrics.jsonl`
- Spatial diagnostic PNG: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/docs/checkpoints/phase-iii-slice-iiie6-4-spatial-distribution.png`
- PNG layout: each row is a scenario in table order; left panel colors unresolved class (`cross_plate_different` red, `third_plate` magenta), right panel colors diagnostic shape (`unique nearest` cyan, `distance tie` blue, process-marked yellow).
- Scenario replay hash: `366ac4465ebe4176`

## Stop Conditions Preserved

- Stop if any `cross_plate_different` or `third_plate` sample is assigned by this slice.
- Stop if manual and automatic remesh paths diverge in policy counters or hold semantics.
- Stop if a same-step held manual remesh increments event count, changes projection/state/crust hashes, or advances the automatic cadence target as though remesh succeeded.
- Stop if process-state cross-references show marked triangles among visible holds; that would redirect the next slice toward IIIB/IIIC/IIID marking rather than a tie-break policy.
- Stop if nearest-hit diagnostics are treated as source-selection authority.

## Recommendation

Use this report to choose the next resolver slice. If `cross_plate_different` holds are mostly unique-nearest with no process marks, the next design can evaluate a nearest-valid-hit lab policy. If process marks appear, fix upstream process-state filtering first. If third-plate or distance ties dominate, keep live remesh held and design a narrower policy or topology correction.
