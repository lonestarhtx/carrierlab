# Phase IIIE.6.5 Post-Motion Nearest-Valid-Hit Tie-Break

**Verdict:** PASS. The slice resolves post-motion `cross_plate_different` and `third_plate` multi-hit holds at the IIIE.3 selector by max-over-all-candidates strict unique-nearest at `1.0e-9 km` tolerance, holds the residual distance ties fail-loud, and preserves the IIIE.6.4 baseline distribution behind `bEnablePhaseIIIE3NearestHitTieBreak = false`.

## Approved Lab Policy

The IIIE.3 selector is extended with a single approved CarrierLab lab policy for post-motion multi-valid different-distance ray hits: nearest-valid-hit tie-break by max-over-all-candidates strict unique-nearest. The rule operates on two IIIE.6.4 multi-hit buckets - `CrossPlateDifferent` and `ThirdPlate` - which the IIIE.6.4 diagnosis at `docs/checkpoints/phase-iii-slice-iiie6-4-live-cadence-post-motion-multihit-diagnosis.md` established are post-motion ray-cast overlap classes arising from plate motion against the duplicated-per-plate-from-global-TDS architecture both the thesis section 3.2.4 and CGF section 6 prescribe. Distance ties (gap <= 1e-9 km) remain fail-loud and are not resolved by this rule, by IIIE.6.3, or by lower-plate-id fallback.

The thesis section 3.3.2.3 step 2 ray-cast prose (`cc5c6807-079.png`, p.68) presupposes uniqueness of the per-sample plate intersection ("le triangle intersecte", "la plaque intersectee") without naming a winner rule for the multi-valid hit case. The CGF 2019 paper section 6 is more compressed than the thesis and adds nothing not already in the thesis prose. **This is not a paper-cited rule and is not a claim of paper-faithfulness.**

Driftworld Tectonics, used as secondary implementation precedent in IIIE.6.3, structurally differs from CarrierLab here: Driftworld resolves multi-overlap via a precomputed continental-priority plate rank (`Assets/Scripts/Planet.cs:937` `CalculatePlatesVP()`, `Assets/Shaders/CSVertexDataInterpolation.compute:280-289` `CSCrustToData`), not by per-sample ray distance at remesh time. CarrierLab's choice of nearest-distance for post-motion overlap is a divergence justified by the per-sample geometry of the IIIE.3 remesh path; it is not a Driftworld-cited rule.

Three-plate cases are resolved by max-over-all-candidates unique-nearest, not by pairwise single-elimination. Max-over-all is associative and order-independent, while pairwise elimination can introduce ordering artifacts at sub-tolerance distance differences. Sample-level splitting remains explicitly out of scope: section 3.3.2.3 step 3 partitions the global TDS by single per-vertex plate index, matching CarrierLab's `FCarrierLabPhaseIIIE5RemeshVertexRecord.AssignedPlateId` contract.

Records resolved by the rule carry `bUsedNearestHitTieBreak = true` with `NearestHitResultClass` (which sub-class), `NearestHitGapKm` (the strict unique-nearest gap), and per-record candidate evidence. Records held by the rule (distance-tie or unsupported bucket) carry `bUsedNearestHitTieBreak = false` and the appropriate sub-enum class, and continue to be counted in `UnresolvedMultiHitCount`. The forbidden-policy counters `bUsedPolicyWinner`, `bUsedPriorOwnerFallback`, the projection-authority counter, and the IIIE.6.3 `bUsedSharedBoundaryTieBreak` counter remain zero on every nearest-hit-resolved record.

## Probe Fixtures

| Fixture | Result | Evidence |
|---|---|---|
| cross-plate different - strict unique nearest resolves | pass | resolved `1`, plate `0`, bucket `cross_plate_different`, nearest_result `unique_nearest_cross_plate_different`, gap `0.0001` km, candidates `2`, distinct plates `2`, shared/nearest/policy/prior `0/1/0/0` |
| third plate - strict unique nearest resolves | pass | resolved `1`, plate `2`, bucket `third_plate`, nearest_result `unique_nearest_third_plate`, gap `5e-05` km, candidates `3`, distinct plates `3`, shared/nearest/policy/prior `0/1/0/0` |
| cross-plate different - distance tie at tolerance held | pass | resolved `0`, plate `-1`, bucket `cross_plate_different`, nearest_result `distance_tie_held`, gap `5e-10` km, candidates `3`, distinct plates `2`, shared/nearest/policy/prior `0/0/0/0` |
| third plate - distance tie at tolerance held | pass | resolved `0`, plate `-1`, bucket `third_plate`, nearest_result `distance_tie_held`, gap `5e-10` km, candidates `3`, distinct plates `3`, shared/nearest/policy/prior `0/0/0/0` |
| cross-plate equal - IIIE.6.5 declines, IIIE.6.3 fires | pass | resolved `1`, plate `10`, bucket `cross_plate_equal`, nearest_result `not_applied`, gap `0` km, candidates `0`, distinct plates `0`, shared/nearest/policy/prior `1/0/0/0` |
| mixed material - strict unique nearest resolves | pass | resolved `1`, plate `12`, bucket `mixed_material`, nearest_result `unique_nearest_mixed_material`, gap `0.0001` km, candidates `2`, distinct plates `2`, shared/nearest/policy/prior `0/1/0/0` |
| mixed material - extension disabled preserves IIIE.6.10 held baseline | pass | resolved `0`, plate `-1`, bucket `mixed_material`, nearest_result `unsupported_held`, gap `0` km, candidates `2`, distinct plates `2`, shared/nearest/policy/prior `0/0/0/0` |

## Default 100k/40 seed-42 step 32 Scenarios

| Scenario | Result | Evidence |
|---|---|---|
| default_100k_40_manual_step_32 | pass | unresolved `4`, crossDiff bucket `16144`, third bucket `8456`, crossDiff resolved `16141`, third resolved `8455`, distance-tie held `4`, unsupported held `0`, shared `0`, coalesced `0`, co-fire `0`, policy/prior `0/0`, events `0->0`, next `64->64`, mode `phase_iii_e6_live_hold_unresolved_multi_hit_4`, hashes proj `0be461d212b1a54a -> 0be461d212b1a54a`, state `ad21171e53f48d79 -> ad21171e53f48d79`, crust `83de07ebd0edbf2a -> 83de07ebd0edbf2a`, selection `310da94dd8e22bb2` |
| default_100k_40_auto_cadence_step_32 | pass | unresolved `4`, crossDiff bucket `16144`, third bucket `8456`, crossDiff resolved `16141`, third resolved `8455`, distance-tie held `4`, unsupported held `0`, shared `0`, coalesced `0`, co-fire `0`, policy/prior `0/0`, events `0->0`, next `32->32`, mode `phase_iii_e6_live_hold_unresolved_multi_hit_4`, hashes proj `113a31bd102e0eb9 -> 0be461d212b1a54a`, state `db7893beb0c6626a -> ad21171e53f48d79`, crust `1d33b3248c817839 -> 83de07ebd0edbf2a`, selection `310da94dd8e22bb2` |
| default_100k_40_iiie6_4_baseline_replay | pass | unresolved `24600`, crossDiff bucket `16144`, third bucket `8456`, crossDiff resolved `0`, third resolved `0`, distance-tie held `0`, unsupported held `0`, shared `0`, coalesced `0`, co-fire `0`, policy/prior `0/0`, events `0->0`, next `64->64`, mode `phase_iii_e6_live_hold_unresolved_multi_hit_24600`, hashes proj `0be461d212b1a54a -> 0be461d212b1a54a`, state `ad21171e53f48d79 -> ad21171e53f48d79`, crust `83de07ebd0edbf2a -> 83de07ebd0edbf2a`, selection `75e0d470dc6841bd` |
| default_100k_40_manual_step_32_replay | pass | unresolved `4`, crossDiff bucket `16144`, third bucket `8456`, crossDiff resolved `16141`, third resolved `8455`, distance-tie held `4`, unsupported held `0`, shared `0`, coalesced `0`, co-fire `0`, policy/prior `0/0`, events `0->0`, next `64->64`, mode `phase_iii_e6_live_hold_unresolved_multi_hit_4`, hashes proj `0be461d212b1a54a -> 0be461d212b1a54a`, state `ad21171e53f48d79 -> ad21171e53f48d79`, crust `83de07ebd0edbf2a -> 83de07ebd0edbf2a`, selection `310da94dd8e22bb2` |

Same-seed replay: hashes `310da94dd8e22bb2` and `310da94dd8e22bb2`, equal.
Manual vs automatic step-32 equivalence (post-state hashes + held counters + remesh mode): match.
IIIE.6.4 baseline replay reproduces the historical `24600` post-motion holds with `bEnablePhaseIIIE3NearestHitTieBreak = false`; the new schema-additive selection hash for the disabled-path is `75e0d470dc6841bd` and the prior schema's `0be461d212b1a54a / ad21171e53f48d79 / 83de07ebd0edbf2a` projection/state/crust still reproduces verbatim because IIIE.6.5's per-record fields are zero on the disabled path.

## Live Remesh Disposition (acceptance criterion 11)

- `UnresolvedMultiHitCount` = `4` after IIIE.6.5; with any tie remaining, the live event MUST stay held. Result: events `0 -> 0` and projection/state/crust hashes unchanged. `LastRemeshMode = phase_iii_e6_live_hold_unresolved_multi_hit_4`.
- IIIE.6.5 is a blocker-reduction slice (24,600 -> ~4) but is NOT a live-visual unblock by itself; live remesh continues to hold while any unresolved tie remains. Final live unblock is a future slice (IIIE.6.6 or later) that names a policy for the residual distance-tie class, or upstream geometry that eliminates the ties. IIIE.6.5 does not pretend to make that decision.

## Forbidden Policy Counters

- `bUsedPolicyWinner`: `0` across the manual_step_32 audit.
- `bUsedPriorOwnerFallback`: `0`.
- IIIE.6.3 + IIIE.6.5 co-fire records: `0`.
- Projection-authority counter is sourced from the IIIE.5 topology rebuild path and remains zero by construction in this slice.

## Hash Regression Strategy

- Schema-additive: `FCarrierLabPhaseIIIE3SelectionRecord` gained `bUsedNearestHitTieBreak`, `NearestHitResultClass`, `NearestHitGapKm`, `NearestHitToleranceKm`, `NearestHitCandidateCount`, `NearestHitDistinctPlateCount`, `NearestHitProcessMarkedRefCount`; later IIIE.6.6 added zero-valued distance-tie fallback fields while this commandlet keeps that resolver disabled. The `ComputePhaseIIIE3SelectionHash` version string is now `CarrierLab-IIIE3-filtered-selection-v4-distance-tie-fallback`.
- Prior IIIE.6.4 manual_step_32 selection hash (v2): `0be461d212b1a54a / ad21171e53f48d79 / 83de07ebd0edbf2a` (projection/state/crust). New IIIE.6.4 baseline selection hash (v4, IIIE.6.5 disabled, IIIE.6.6 zero-valued): `75e0d470dc6841bd`. The hashes differ from schema/mixer changes; behaviour is identical.
- IIIE.6.5 default 100k/40 seed-42 manual_step_32 selection hash (v4, IIIE.6.5 enabled and IIIE.6.6 disabled): `310da94dd8e22bb2`. This remains the historical nearest-hit blocker-reduction evidence with the residual 4 ties held.

## Stop Conditions Preserved

- Stop if any `bUsedPolicyWinner`, `bUsedPriorOwnerFallback`, or projection-authority counter becomes nonzero on any record or in any aggregate.
- Stop if any record co-fires `bUsedSharedBoundaryTieBreak` and `bUsedNearestHitTieBreak`. The two resolvers handle structurally distinct geometric classes and must remain mutually exclusive.
- Stop if `ApplyPhaseIIIELiveRemeshEvent` advances events or mutates crust/projection/state hashes while `UnresolvedMultiHitCount > 0`.
- Stop if the IIIE.6.4 baseline replay distribution does not reproduce 24,600 holds at default 100k/40 step 32 with `bEnablePhaseIIIE3NearestHitTieBreak = false`.
- Stop if upstream IIIE gate hashes drift on slices that do not consume the IIIE.3 selection record (IIIE.1 through IIIE.5 are untouched).
- Stop if distance ties are silently routed to IIIE.6.3 lower-plate-id fallback or any other broad resolver. Distance ties remain fail-loud.

## Artifacts

- JSONL metrics: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIE65NearestHit/metrics.jsonl`

## Next Slice Boundary

Final live-visual unblocking is out of scope for IIIE.6.5. The residual ~4 distance-tie holds keep `ApplyPhaseIIIELiveRemeshEvent` on the fail-loud branch. IIIE.6.6 (or later) names a policy for the distance-tie class, or upstream geometry eliminates the ties. Until then, the live remesh continues to surface the same fail-loud mode (`phase_iii_e6_live_hold_unresolved_multi_hit_4`) on default 100k/40 seed-42 step 32, and the actor's IIIE summary layer reports the same crust/projection hashes before and after a live remesh attempt.
