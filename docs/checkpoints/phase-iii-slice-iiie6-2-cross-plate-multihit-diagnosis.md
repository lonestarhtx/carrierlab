# Phase IIIE.6.2 Cross-Plate Multi-Hit Diagnosis

Verdict: PASS / DIAGNOSTIC-ONLY CROSS-PLATE HOLD CLASSIFICATION. This hardening slice explains the remaining default IIIE.6 live remesh holds after same-plate duplicate coalescing. It does not resolve, coalesce, skip, or apply any cross-plate or third-plate multi-hit.

## Scope

- Selector behavior is unchanged: cross-plate equality and third-plate hits remain fail-loud unresolved records.
- Candidate snapshots are diagnostic evidence only; global sample ids and source triangle ids are not remesh authority.
- No centroid, random, synthetic, prior-owner, projection-derived, or cross-plate winner rule is introduced.

## Gates

| Gate | Result | Evidence |
|---|---:|---|
| Fixture `two-plate same source triangle` | pass | selection `unresolved same-material multi-hit`, bucket `cross-plate-equal`, shape `two_plate_same_source_triangle`, candidates `2`, policy/prior `0/0`, hash `227dab5b27a5a072`. |
| Fixture `two-plate shared global edge` | pass | selection `unresolved same-material multi-hit`, bucket `cross-plate-equal`, shape `two_plate_shared_global_edge`, candidates `2`, policy/prior `0/0`, hash `d50bd92207a42d17`. |
| Fixture `two-plate shared global vertex only` | pass | selection `unresolved same-material multi-hit`, bucket `cross-plate-equal`, shape `two_plate_shared_global_vertex_only`, candidates `2`, policy/prior `0/0`, hash `ab018b55b7ca573f`. |
| Fixture `two-plate no shared global vertices` | pass | selection `unresolved same-material multi-hit`, bucket `cross-plate-equal`, shape `two_plate_no_shared_global_vertices`, candidates `2`, policy/prior `0/0`, hash `572f3cd7aa8b8a83`. |
| Fixture `two-plate field mismatch` | pass | selection `unresolved same-material multi-hit`, bucket `cross-plate-equal`, shape `two_plate_field_mismatch`, candidates `2`, policy/prior `0/0`, hash `56347b8957f36f6e`. |
| Fixture `three-plate common global vertex` | pass | selection `unresolved third-plate multi-hit`, bucket `third-plate`, shape `three_plate_common_global_vertex`, candidates `3`, policy/prior `0/0`, hash `ec3e934bcb1bb4c8`. |
| Fixture `three-plate edge plus intruder` | pass | selection `unresolved third-plate multi-hit`, bucket `third-plate`, shape `three_plate_edge_plus_intruder`, candidates `3`, policy/prior `0/0`, hash `afce71f415fd926e`. |
| Fixture `three-plate no common source vertex` | pass | selection `unresolved third-plate multi-hit`, bucket `third-plate`, shape `three_plate_no_common_source_vertex`, candidates `3`, policy/prior `0/0`, hash `d7e6b55a93f1e80f`. |
| Fixture `non-boundary interior overlap` | pass | selection `unresolved same-material multi-hit`, bucket `cross-plate-equal`, shape `non_boundary_or_interior_overlap`, candidates `2`, policy/prior `0/0`, hash `745a10fb4280d81a`. |
| Default 100k/40 replay A | pass | samples `100000`, unresolved `5845`, cross/third `5761/84`, coalesced `58589`, shapes same-source/shared-edge/shared-vertex/no-shared/field-mismatch/three-common/three-edge-intruder/three-none/non-boundary/invalid `0/3740/2021/0/0/84/0/0/0/0`, policy/prior/projection `0/0/0`, selection `6da5cc74b3713bd4`, diagnosis `ca9995af2500639c`. |
| Default 100k/40 replay B | pass | samples `100000`, unresolved `5845`, cross/third `5761/84`, coalesced `58589`, shapes same-source/shared-edge/shared-vertex/no-shared/field-mismatch/three-common/three-edge-intruder/three-none/non-boundary/invalid `0/3740/2021/0/0/84/0/0/0/0`, policy/prior/projection `0/0/0`, selection `6da5cc74b3713bd4`, diagnosis `ca9995af2500639c`. |
| Same-seed diagnosis replay | pass | selection hash stable `1`, diagnosis hash stable `1`. |

## Distribution

| Shape | Count |
|---|---:|
| `two_plate_same_source_triangle` | 0 |
| `two_plate_shared_global_edge` | 3740 |
| `two_plate_shared_global_vertex_only` | 2021 |
| `two_plate_no_shared_global_vertices` | 0 |
| `two_plate_field_mismatch` | 0 |
| `three_plate_common_global_vertex` | 84 |
| `three_plate_edge_plus_intruder` | 0 |
| `three_plate_no_common_source_vertex` | 0 |
| `non_boundary_or_interior_overlap` | 0 |
| `invalid_or_unclassified` | 0 |

## Recommendation

- Dominant class is `shared global edge boundary duplication` with `3740` records; the next slice should target that class without relaxing cross-plate holds generically.
- IIIE consolidation remains blocked on a real cross-plate/third-plate decision; this report only tells us which decision should be made next.

## Stop Conditions

- Stop if any cross-plate or third-plate hold is resolved by this diagnostic slice.
- Stop if policy/prior/projection counters become nonzero.
- Stop if the default live cadence is described as fully unblocked by this diagnostic-only report.

Metrics: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIE62CrossPlateMultiHit/phase-iii-slice-iiie6-2-cross-plate-multihit-diagnosis-metrics.jsonl`.
