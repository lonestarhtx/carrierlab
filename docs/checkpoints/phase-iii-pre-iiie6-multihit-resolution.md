# Phase III Pre-IIIE.6.1 Multi-Hit Resolution Audit

Verdict: PASS / SAME-PLATE DUPLICATE COALESCING ENABLED; TRUE CROSS-PLATE AND THIRD-PLATE HOLDS REMAIN LOUD. The audit first measured the current live unresolved distribution, then enabled only geometry-equivalent same-plate duplicate cleanup because the single-plate blocker was 100% `within-plate-coincident` with zero field-mismatch holds.

## Scope

- This checkpoint diagnoses unresolved post-filter IIIE.3 multi-hit records without applying a remesh winner policy.
- No centroid, random, synthetic, prior-owner, or projection-derived source selection is introduced.
- Tolerances are explicit: `1e-9 km` ray-distance coincidence, `1e-9` scalar fields, `1e-9 km` elevation fields, and `1e-8` unit-vector fields, matching the IIIE.2 edge-t / IIIE.4 oracle residual tolerance family.

## Gates

| Gate | Result | Evidence |
|---|---:|---|
| Audit-only same-plate coincident edge duplicate | pass | bucket `within-plate-coincident`, selection `unresolved same-material multi-hit`, mismatch `0`, residuals ray/scalar/elev/vector `2.5e-10/0/0/0`, policy/prior `0/0`, hash `2d5bb21c7749b103`. |
| Audit-only same-plate vertex fan duplicate | pass | bucket `within-plate-coincident`, selection `unresolved same-material multi-hit`, mismatch `0`, residuals ray/scalar/elev/vector `0/0/0/0`, policy/prior `0/0`, hash `ccf25f76dbf5ef55`. |
| Audit-only same-plate field mismatch duplicate | pass | bucket `within-plate-coincident`, selection `unresolved same-material multi-hit`, mismatch `1`, residuals ray/scalar/elev/vector `0/0/0.0001/0`, policy/prior `0/0`, hash `f1b6d5486a25f3c6`. |
| Audit-only same-plate distance-separated duplicate | pass | bucket `within-plate-distance-separated`, selection `unresolved same-material multi-hit`, mismatch `0`, residuals ray/scalar/elev/vector `63.7/0/0/0`, policy/prior `0/0`, hash `58882380d5c78f7c`. |
| Audit-only cross-plate equal-distance duplicate | pass | bucket `cross-plate-equal`, selection `unresolved same-material multi-hit`, mismatch `0`, residuals ray/scalar/elev/vector `0/0/0/0`, policy/prior `0/0`, hash `07795be7264ee12e`. |
| Audit-only cross-plate different-distance duplicate | pass | bucket `cross-plate-different`, selection `unresolved same-material multi-hit`, mismatch `0`, residuals ray/scalar/elev/vector `63.7/0/0/0`, policy/prior `0/0`, hash `a551f4f9241a527d`. |
| Audit-only mixed-material multi-hit | pass | bucket `mixed-material`, selection `unresolved mixed-material multi-hit`, mismatch `0`, residuals ray/scalar/elev/vector `0/1/4/0`, policy/prior `0/0`, hash `7842297bf02d1bd9`. |
| Audit-only third-plate multi-hit | pass | bucket `third-plate`, selection `unresolved third-plate multi-hit`, mismatch `0`, residuals ray/scalar/elev/vector `0/0/0/0`, policy/prior `0/0`, hash `e54effea5cefefc2`. |
| Single-plate live pre-coalescing distribution | pass | samples/plates `96/1`, unresolved `73`, coalesced `0`, buckets within-coincident/distance/cross-eq/cross-diff/mixed/third `73/0/0/0/0/0`, mismatch holds `0`, max residuals ray/scalar/elev/vector `2.83e-12/0/0/0`, coincident share `100.00%`, proceed `1`, hash `e7c170295363cc74`. |
| Default multi-plate pre-coalescing diagnostic | pass | samples/plates `100000/40`, unresolved `64434`, coalesced `0`, buckets within-coincident/distance/cross-eq/cross-diff/mixed/third `58589/0/5761/0/0/84`, mismatch holds `0`, max residuals ray/scalar/elev/vector `4.24e-12/0/0/0`, coincident share `90.93%`, proceed `1`, hash `9c4381ad2f3bed71`. |

| Coalescing same-plate coincident edge duplicate | pass | bucket `within-plate-coincident`, selection `resolved single hit`, coalesced `1`, effective/coalesced-candidates `1/1`, mismatch `0`, policy/prior `0/0`, hash `56819c24e9524f3a`. |
| Coalescing same-plate vertex fan duplicate | pass | bucket `within-plate-coincident`, selection `resolved single hit`, coalesced `1`, effective/coalesced-candidates `1/2`, mismatch `0`, policy/prior `0/0`, hash `326778e1f918e2fc`. |
| Coalescing same-plate field mismatch duplicate | pass | bucket `within-plate-coincident`, selection `unresolved same-material multi-hit`, coalesced `0`, effective/coalesced-candidates `2/0`, mismatch `1`, policy/prior `0/0`, hash `f1b6d5486a25f3c6`. |
| Coalescing same-plate distance-separated duplicate | pass | bucket `within-plate-distance-separated`, selection `unresolved same-material multi-hit`, coalesced `0`, effective/coalesced-candidates `2/0`, mismatch `0`, policy/prior `0/0`, hash `58882380d5c78f7c`. |
| Coalescing cross-plate equal-distance duplicate | pass | bucket `cross-plate-equal`, selection `unresolved same-material multi-hit`, coalesced `0`, effective/coalesced-candidates `2/0`, mismatch `0`, policy/prior `0/0`, hash `07795be7264ee12e`. |
| Coalescing cross-plate different-distance duplicate | pass | bucket `cross-plate-different`, selection `unresolved same-material multi-hit`, coalesced `0`, effective/coalesced-candidates `2/0`, mismatch `0`, policy/prior `0/0`, hash `a551f4f9241a527d`. |
| Coalescing mixed-material multi-hit | pass | bucket `mixed-material`, selection `unresolved mixed-material multi-hit`, coalesced `0`, effective/coalesced-candidates `2/0`, mismatch `0`, policy/prior `0/0`, hash `7842297bf02d1bd9`. |
| Coalescing third-plate multi-hit | pass | bucket `third-plate`, selection `unresolved third-plate multi-hit`, coalesced `0`, effective/coalesced-candidates `3/0`, mismatch `0`, policy/prior `0/0`, hash `e54effea5cefefc2`. |
| Single-plate live apply after coalescing | pass | applied `1`, events `0->1`, unresolved `0`, coalesced `73`, policy `0`, mode `phase_iii_e6_live gen=0 applied=0 rift_pending=0 coalesced=73 shared_tiebreak=0 majority=0 tj_split=0`, crust `8b7758c51944a162->5c75dc9d238d9011`. |
| Default multi-plate post-coalescing diagnostic | pass | samples/plates `100000/40`, unresolved `5845`, coalesced `58589`, buckets within-coincident/distance/cross-eq/cross-diff/mixed/third `58589/0/5761/0/0/84`, mismatch holds `0`, max residuals ray/scalar/elev/vector `4.24e-12/0/0/0`, coincident share `90.93%`, proceed `0`, hash `ffff08c4f12fd4ab`. |

## Decision Rule

- Proceed to same-plate coalescing: `yes`.
- Coalescing is active only for `within-plate-coincident` records that also pass field-equivalence residual gates.
- Default multi-plate live remesh can still hold on true cross-plate equal or third-plate records; those are not resolved in this slice.

## Stop Conditions

- Stop if cross-plate, mixed-material, or third-plate records are resolved by this classifier.
- Stop if field-mismatched same-plate coincident records are coalesced.
- Stop if `bUsedPolicyWinner` or `bUsedPriorOwnerFallback` becomes nonzero.

Metrics: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/PreIIIE6MultiHit/phase-iii-pre-iiie6-multihit-resolution-metrics.jsonl`.
