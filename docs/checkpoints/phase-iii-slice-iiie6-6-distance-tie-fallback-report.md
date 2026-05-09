# Phase IIIE.6.6 Distance-Tie Last-Resort Fallback Report

**Verdict:** PASS. IIIE.6.6 resolves only the residual IIIE.6.5 `DistanceTieHeld` records using the approved continental-priority -> older-oceanic-age -> lower-plate-id hierarchy. The selection gate is expected to clear `UnresolvedMultiHitCount == 0`; any later live-apply invalid-record hold is reported separately and is not treated as distance-tie success. This is a named CarrierLab lab policy, not paper-cited thesis behavior.

## Scope

- The fallback fires only when IIIE.6.5 classified a record as `DistanceTieHeld`; disabling IIIE.6.5 preserves the IIIE.6.4 baseline instead of handing resolution to IIIE.6.6.
- The `1e-9 km` tolerance is a micron-scale tie window. It is a numerical-coincidence last resort, not a broad remesh ownership policy.
- Partial-apply remains rejected because carrying old sample state forward would be a prior-owner fallback in IIIE.1 terms.
- This commandlet is intentionally selection-only at default scale. The live-apply path is an independent expensive gate and must not be inferred from selection success.
- Metrics JSONL: C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIE66DistanceTieFallback/metrics.jsonl

## Fixture Gates

| Fixture | Result | Evidence |
|---|---|---|
| distance tie fallback layer 1 - continental priority | pass | resolved `1`, plate `9`, bucket `cross_plate_different`, nearest `distance_tie_held`, fallback `continental_priority`, flags shared/nearest/dtie `0/0/1`, policy/prior `0/0` |
| distance tie fallback layer 2 - older oceanic age | pass | resolved `1`, plate `8`, bucket `cross_plate_different`, nearest `distance_tie_held`, fallback `older_oceanic_age`, flags shared/nearest/dtie `0/0/1`, policy/prior `0/0` |
| distance tie fallback layer 3 - lower plate id | pass | resolved `1`, plate `2`, bucket `cross_plate_different`, nearest `distance_tie_held`, fallback `lower_plate_id`, flags shared/nearest/dtie `0/0/1`, policy/prior `0/0` |
| third-plate distance tie fallback layer 1 - max over all candidates | pass | resolved `1`, plate `4`, bucket `third_plate`, nearest `distance_tie_held`, fallback `continental_priority`, flags shared/nearest/dtie `0/0/1`, policy/prior `0/0` |
| distance tie fallback opt-out preserves IIIE.6.5 hold | pass | resolved `0`, plate `-1`, bucket `cross_plate_different`, nearest `distance_tie_held`, fallback `none`, flags shared/nearest/dtie `0/0/0`, policy/prior `0/0` |

## Default Scale Scenarios

| Scenario | Result | Evidence |
|---|---|---|
| manual_step_10_enabled_selection | pass | apply `0`, step `10->10`, events `0->0`, unresolved `0`, nearest cross/third/tie `22697/1578/0`, fallback total/cross/third `2/2/0`, fallback layers `0/0/2`, cross layers `0/0/2`, third layers `0/0/0`, invalid/gen/applied/rift/noBoundary/nonsep `0/0/0/0/0/0`, cofire `0`, policy/prior `0/0`, mode `selection_only`, hashes proj `182d62768db7c39d->182d62768db7c39d`, state `c6e5f90b815805f2->c6e5f90b815805f2`, crust `392976b507028224->392976b507028224`, selection `07f76a10a609d6ec` |
| manual_step_20_enabled_selection | pass | apply `0`, step `20->20`, events `0->0`, unresolved `0`, nearest cross/third/tie `22186/4848/0`, fallback total/cross/third `4/3/1`, fallback layers `0/0/4`, cross layers `0/0/3`, third layers `0/0/1`, invalid/gen/applied/rift/noBoundary/nonsep `0/0/0/0/0/0`, cofire `0`, policy/prior `0/0`, mode `selection_only`, hashes proj `7494ba96b24c42e6->7494ba96b24c42e6`, state `9da595fa3542fed2->9da595fa3542fed2`, crust `49c61d9444220b16->49c61d9444220b16`, selection `8ddcfee72c7a00e4` |
| manual_step_32_enabled_selection | pass | apply `0`, step `32->32`, events `0->0`, unresolved `0`, nearest cross/third/tie `16141/8455/0`, fallback total/cross/third `4/3/1`, fallback layers `0/0/4`, cross layers `0/0/3`, third layers `0/0/1`, invalid/gen/applied/rift/noBoundary/nonsep `0/0/0/0/0/0`, cofire `0`, policy/prior `0/0`, mode `selection_only`, hashes proj `0be461d212b1a54a->0be461d212b1a54a`, state `ad21171e53f48d79->ad21171e53f48d79`, crust `83de07ebd0edbf2a->83de07ebd0edbf2a`, selection `aa185b5575b5c8a4` |
| manual_step_32_replay_selection | pass | apply `0`, step `32->32`, events `0->0`, unresolved `0`, nearest cross/third/tie `16141/8455/0`, fallback total/cross/third `4/3/1`, fallback layers `0/0/4`, cross layers `0/0/3`, third layers `0/0/1`, invalid/gen/applied/rift/noBoundary/nonsep `0/0/0/0/0/0`, cofire `0`, policy/prior `0/0`, mode `selection_only`, hashes proj `0be461d212b1a54a->0be461d212b1a54a`, state `ad21171e53f48d79->ad21171e53f48d79`, crust `83de07ebd0edbf2a->83de07ebd0edbf2a`, selection `aa185b5575b5c8a4` |
| manual_step_60_enabled_selection | pass | apply `0`, step `60->60`, events `0->0`, unresolved `0`, nearest cross/third/tie `16278/10168/0`, fallback total/cross/third `5/2/3`, fallback layers `0/0/5`, cross layers `0/0/2`, third layers `0/0/3`, invalid/gen/applied/rift/noBoundary/nonsep `0/0/0/0/0/0`, cofire `0`, policy/prior `0/0`, mode `selection_only`, hashes proj `705a7827cadbd691->705a7827cadbd691`, state `4c7bdbfd6a6469cd->4c7bdbfd6a6469cd`, crust `3db874db54013fce->3db874db54013fce`, selection `65b2829ee8ac8aba` |
| iiie6_5_baseline_distance_fallback_disabled | pass | apply `0`, step `32->32`, events `0->0`, unresolved `4`, nearest cross/third/tie `16141/8455/4`, fallback total/cross/third `0/0/0`, fallback layers `0/0/0`, cross layers `0/0/0`, third layers `0/0/0`, invalid/gen/applied/rift/noBoundary/nonsep `0/0/0/0/0/0`, cofire `0`, policy/prior `0/0`, mode `selection_only`, hashes proj `0be461d212b1a54a->0be461d212b1a54a`, state `ad21171e53f48d79->ad21171e53f48d79`, crust `83de07ebd0edbf2a->83de07ebd0edbf2a`, selection `ef0d6cc22e1dfe40` |
| iiie6_4_baseline_nearest_disabled | pass | apply `0`, step `32->32`, events `0->0`, unresolved `24600`, nearest cross/third/tie `0/0/0`, fallback total/cross/third `0/0/0`, fallback layers `0/0/0`, cross layers `0/0/0`, third layers `0/0/0`, invalid/gen/applied/rift/noBoundary/nonsep `0/0/0/0/0/0`, cofire `0`, policy/prior `0/0`, mode `selection_only`, hashes proj `0be461d212b1a54a->0be461d212b1a54a`, state `ad21171e53f48d79->ad21171e53f48d79`, crust `83de07ebd0edbf2a->83de07ebd0edbf2a`, selection `a61a52fc310e6597` |

Same-seed manual step-32 replay: **pass**.

## Baseline Preservation

- IIIE.6.4 baseline: disabling nearest-hit also prevents IIIE.6.6 from firing because the fallback is gated on IIIE.6.5's `DistanceTieHeld` classification; default step 32 remains at 24,600 holds.
- IIIE.6.5 baseline: enabling nearest-hit but disabling distance-tie fallback preserves the 4 residual holds at default step 32.
- Default IIIE.6.6: all resolver flags enabled, default manual selection reports 0 unresolved multi-hit holds. Live apply remains an independent gate and must not be reported as successful from this selection-only evidence.

## Stop Conditions

- Stop if `bUsedDistanceTieFallback` appears without `NearestHitResultClass == DistanceTieHeld`.
- Stop if any record co-fires shared-boundary, nearest-hit, and/or distance-tie positive resolver flags.
- Stop if forbidden-policy counters increment.
- Stop if either historical baseline cannot be reproduced with its opt-out flag composition.

## Next Slice Boundary

Next is a narrow live-apply / divergent-gap record-builder diagnostic if the editor still holds or spends too long inside `ApplyPhaseIIIELiveRemeshEvent`; otherwise IIIE consolidation can rerun and disclose the named IIIE lab policies, verify the live actor visually mutates through the IIIE path without Stage 1.5 fallback, and measure the integrated paper Table 2 cost ratio.
