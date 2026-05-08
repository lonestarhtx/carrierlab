# Phase IIIE.6.3 Shared-Boundary Multi-Hit Tie-Break

Verdict: PASS. The slice resolves only the IIIE.6.2 boundary-shared same-distance classes with an approved named lab policy; true non-boundary, field-mismatch, or unclassified multi-hits remain fail-loud.

## Scope

- This is a CarrierLab lab policy because the thesis/CGF remesh prose is silent on multiple valid same-distance boundary hits.
- The rule is hierarchical: higher plate-level continental fraction, then older plate-level oceanic age, then lower plate id.
- Driftworld is treated as secondary implementation precedent for continental-priority overlap handling, not as paper authority.
- `bUsedPolicyWinner`, prior-owner fallback, and projection authority remain forbidden counters and must stay zero.

## Gate Table

| Gate | Result | Evidence |
|---|---|---|
| two-plate shared edge continental-priority | pass | resolved `1`, plate `0`, shape `two_plate_shared_global_edge`, rule `continental_priority`, shared/policy/prior `1/0/0`. |
| two-plate shared vertex older-oceanic-age | pass | resolved `1`, plate `3`, shape `two_plate_shared_global_vertex_only`, rule `older_oceanic_age`, shared/policy/prior `1/0/0`. |
| two-plate shared edge lower-plate-id | pass | resolved `1`, plate `4`, shape `two_plate_shared_global_edge`, rule `lower_plate_id`, shared/policy/prior `1/0/0`. |
| three-plate common vertex continental max | pass | resolved `1`, plate `7`, shape `three_plate_common_global_vertex`, rule `continental_priority`, shared/policy/prior `1/0/0`. |
| three-plate common vertex lower-id associative | pass | resolved `1`, plate `9`, shape `three_plate_common_global_vertex`, rule `lower_plate_id`, shared/policy/prior `1/0/0`. |
| two-plate field mismatch remains held | pass | resolved `0`, plate `-1`, shape `two_plate_field_mismatch`, rule `none`, shared/policy/prior `0/0/0`. |
| two-plate no shared vertices remains held | pass | resolved `0`, plate `-1`, shape `two_plate_no_shared_global_vertices`, rule `none`, shared/policy/prior `0/0/0`. |
| non-boundary interior overlap remains held | pass | resolved `0`, plate `-1`, shape `non_boundary_or_interior_overlap`, rule `none`, shared/policy/prior `0/0/0`. |
| Default 100000/40 seed-42 live selection | pass | unresolved `0`, coalesced `58589`, shared tie-break `5845`, cross/third `5761/84`, shapes edge/vertex/three `3740/2021/84`, rules cont/age/id `3885/0/1960`, policy/prior `0/0`, hash `f218dda57d6868b3`. |
| Same-seed replay | pass | hashes `f218dda57d6868b3` and `f218dda57d6868b3`. |

## Policy Disclosure

IIIE.6.3 adds a fifth named IIIE lab policy for consolidation: shared-boundary same-distance multi-hit tie-break. The policy is engineering-defensible but not paper-cited. Consolidation must disclose it alongside zGamma sqrt-subsidence, 2-of-3 majority, triangle-level triple-junction centroid split, and continental overwrite -> rifting-pending.

## Stop Conditions

- Stop if shared-boundary tie-break increments `bUsedPolicyWinner`, prior-owner fallback, or projection-authority counters.
- Stop if field-mismatch, no-shared-vertices, interior/non-boundary, or unclassified holds are resolved by this rule.
- Stop if IIIE.6.2 baseline diagnosis cannot still be reproduced with the tie-break disabled.

## Next Slice Boundary

With IIIE.6.3 applied, default live remesh is no longer blocked by legitimate shared-boundary multi-hits. The next work should verify the actor visual path and then proceed to IIIE consolidation only if live remesh mutation remains coherent under the contact-sheet and commandlet gates.
