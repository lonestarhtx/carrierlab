# Phase IIIE.4 Divergent Oceanic Field Generation

Verdict: PASS / IIIE.5 UNBLOCKED. This slice generates audit-only oceanic fields for IIIE.3 divergent gap routes. It does not rebuild topology, mutate the global remesh TDS, reset process state, optimize replay, or resolve multi-hit samples.

## Scope

- IIIE.4 consumes only `NoHitDivergentGap` and `DivergentGapAfterFiltering` records from IIIE.3.
- It obtains q1/q2/qGamma from the IIIE.2 continuous boundary query and records q1/q2 plate ids, edge ids, qGamma, signed separating velocity, generated elevation, age, and ridge direction.
- Generated divergent crust has `OceanicAge = 0` and `RidgeDirection = retangent(normalize((p - qGamma) x p))`.
- Elevation follows the local extraction contract `z = alpha * zBar(p) + (1 - alpha) * zGamma(p)`, with `alpha = dGamma / (dGamma + dPlate)`.
- `zBar(p)` is compared against fixture-owned expected constants for distance interpolation between q1/q2 boundary elevations. `zGamma(p)` remains a named IIIE.4 placeholder/deviation: thesis section 3.3.2.1 names a ridge profile and section 3.3.2.3 defines qGamma, but the local extraction has no closed-form zGamma curve. The current actor arithmetic keeps the existing linear alpha-parameterized profile anchored to thesis Table 3.2 constants: ridge peak `-1 km`, abyssal plain `-6 km`; it is not claimed as the final paper-faithful ridge profile.
- Resolved single-hit and unresolved multi-hit classes are rejected before field generation. Non-positive signed separating velocity is an anomaly, not a fallback.

## Gates

| Gate | Result | Evidence |
|---|---:|---|
| no-hit divergent oceanic generation | pass | class `no-hit divergent gap`, generated `1`, boundary `1`, anomaly `0`, rejected `0/0`, q1/q2 `0/1`, assigned `0`, signed velocity `0.002`, q dist residuals `1.82e-12/1.82e-12 km`, ridge/nearest residuals `4.09e-12/1.82e-12 km`, qGamma residual `0`, alpha/elev residuals `6.11e-16/2.22e-15`, ridge residual `0`, radial dot `0`, hash `0fc06efc6a92253b`, expected `0fc06efc6a92253b`, match `1`. |
| filter-exhausted divergent oceanic generation | pass | class `divergent gap after filtering`, generated `1`, boundary `1`, anomaly `0`, rejected `0/0`, q1/q2 `0/1`, assigned `0`, signed velocity `0.002`, q dist residuals `1.82e-12/1.82e-12 km`, ridge/nearest residuals `4.09e-12/1.82e-12 km`, qGamma residual `0`, alpha/elev residuals `6.11e-16/2.22e-15`, ridge residual `0`, radial dot `0`, hash `e9052ba3fa60a5dd`, expected `e9052ba3fa60a5dd`, match `1`. |
| non-separating q1/q2 anomaly | pass | class `no-hit divergent gap`, generated `0`, boundary `1`, anomaly `1`, rejected `0/0`, q1/q2 `0/1`, assigned `-1`, signed velocity `-0.002`, q dist residuals `1.82e-12/1.82e-12 km`, ridge/nearest residuals `0/0 km`, qGamma residual `0`, alpha/elev residuals `0/0`, ridge residual `0`, radial dot `0`, hash `91ab557101086280`, expected `91ab557101086280`, match `1`. |
| resolved single-hit route rejected | pass | class `resolved single hit`, generated `0`, boundary `0`, anomaly `0`, rejected `1/0`, q1/q2 `-1/-1`, assigned `-1`, signed velocity `0.002`, q dist residuals `0/0 km`, ridge/nearest residuals `0/0 km`, qGamma residual `0`, alpha/elev residuals `0/0`, ridge residual `0`, radial dot `0`, hash `542d1b822c77773c`, expected `542d1b822c77773c`, match `1`. |
| unresolved multi-hit route rejected | pass | class `unresolved mixed-material multi-hit`, generated `0`, boundary `0`, anomaly `0`, rejected `1/1`, q1/q2 `-1/-1`, assigned `-1`, signed velocity `0.002`, q dist residuals `0/0 km`, ridge/nearest residuals `0/0 km`, qGamma residual `0`, alpha/elev residuals `0/0`, ridge residual `0`, radial dot `0`, hash `251e526a1f992b5e`, expected `251e526a1f992b5e`, match `1`. |
| no two-plate boundary pair anomaly | pass | class `no-hit divergent gap`, generated `0`, boundary `0`, anomaly `0`, rejected `0/0`, q1/q2 `-1/-1`, assigned `-1`, signed velocity `0.002`, q dist residuals `0/0 km`, ridge/nearest residuals `0/0 km`, qGamma residual `0`, alpha/elev residuals `0/0`, ridge residual `0`, radial dot `0`, hash `e5e8edfef4290c4c`, expected `e5e8edfef4290c4c`, match `1`. |
| asymmetric q1/q2 elevation interpolation | pass | class `no-hit divergent gap`, generated `1`, boundary `1`, anomaly `0`, rejected `0/0`, q1/q2 `1/0`, assigned `1`, signed velocity `0.002`, q dist residuals `1.14e-12/2.27e-12 km`, ridge/nearest residuals `0/1.14e-12 km`, qGamma residual `5.55e-17`, alpha/elev residuals `1.11e-16/4.44e-16`, ridge residual `2.49e-16`, radial dot `0`, hash `1db359a93a762f0d`, expected `1db359a93a762f0d`, match `1`. |
| off-axis sample ridge direction | pass | class `no-hit divergent gap`, generated `1`, boundary `1`, anomaly `0`, rejected `0/0`, q1/q2 `1/0`, assigned `1`, signed velocity `0.002`, q dist residuals `4.09e-12/1.82e-12 km`, ridge/nearest residuals `0/4.09e-12 km`, qGamma residual `0`, alpha/elev residuals `5.55e-16/1.33e-15`, ridge residual `0`, radial dot `0`, hash `64f668f4b60e9f9f`, expected `64f668f4b60e9f9f`, match `1`. |
| antipodal qGamma degenerate ridge direction | pass | class `no-hit divergent gap`, generated `1`, boundary `1`, anomaly `0`, rejected `0/0`, q1/q2 `0/1`, assigned `0`, signed velocity `0.002`, q dist residuals `1.82e-12/0 km`, ridge/nearest residuals `0/1.82e-12 km`, qGamma residual `0`, alpha/elev residuals `0/0`, ridge residual `0`, radial dot `0`, hash `7e4b542a3cf2174f`, expected `7e4b542a3cf2174f`, match `1`. |
| Fixture record-hash regression | pass | `9/9` fixture records matched their expected hashes. |
| Same-seed oceanic-generation replay | pass | Replay hashes `0fc06efc6a92253b` and `0fc06efc6a92253b`. |

## Contract Table

| Paper / IIIE.1 requirement | CarrierLab support now | IIIE obligation still ahead | Gate needed |
|---|---|---|---|
| Zero valid hits become divergent gap fill | IIIE.4 accepts only no-hit and filter-exhausted route classes | Wire these records into the actual remesh event in IIIE.5 | End-to-end remesh event fixture with zero-hit route |
| q1/q2 are continuous nearest boundary points on different plates | IIIE.4 calls the IIIE.2 query and records q ids, edges, qGamma, distances, and elevations | Preserve this provenance when topology rebuild duplicates/re-indexes samples | Event-log row carries q1/q2/qGamma through rebuild |
| New oceanic crust age is zero | Generation records `OceanicAge = 0` and gates residual against fixture-owned expected constants | Mutate global samples only inside the remesh event | Generated sample field residuals and record-hash regression |
| Ridge direction is `(p - qGamma) x p` retangented/normalized | Generated records have non-zero tangent ridge vectors with near-zero radial dot | Preserve vector fields through duplicate/re-index/re-compact | Vector magnitude and radial-dot oracle |
| Gap fill requires separating q1/q2 kinematics | Non-positive signed velocity reports an anomaly and does not generate | Use production plate motions at remesh cadence | Positive/negative velocity fixtures in remesh event |
| Multiple valid hits are stop conditions | Resolved and unresolved non-gap classes are rejected before generation | Keep unresolved counts blocking primary remesh | Multi-hit rejection gate remains required |

## Forbidden Policy Checks

| Policy | IIIE.4 status |
|---|---|
| Prior global sample owner/fraction fallback | Not used; every generation record keeps `bUsedPriorOwnerFallback = false`. |
| Centroid/random/synthetic winner policy | Not used; missing boundary pairs stay anomalies. |
| Stage 1.5 endpoint/midpoint q1/q2 authority | Not used; q1/q2 come from IIIE.2 continuous boundary provenance. |
| Recovery/repair/backfill/retention/hysteresis/anchoring | Not used; rejected and anomalous routes remain non-generated. |
| Silent unresolved multi-hit resolution | Forbidden; unresolved routes are rejected before oceanic generation. |

## Stop Conditions For IIIE.5

- Stop if topology rebuild routes resolved single-hit or unresolved multi-hit samples into divergent oceanic generation.
- Stop if a non-positive q1/q2 separating velocity generates oceanic fields.
- Stop if a missing two-plate boundary pair fabricates q1/q2, plate ownership, or elevation.
- Stop if Stage 1.5 prior-owner, endpoint/midpoint, recovery, or anchoring policy becomes authority in the primary IIIE remesh path.
- Stop if IIIE.5 topology rebuild drops q1/q2/qGamma, signed velocity, age, elevation, or ridge-direction evidence from the event log.

## Next Slice Boundary

IIIE.5 should rebuild plate-local topology from the global TDS assignment: duplicate, re-index, and re-compact per plate while preserving motion and the IIIE.4 generated field records. It must also perform the remesh process-state reset contracted in IIIE.1: invalidate subduction marks, active convergence lists, distance-to-front records, and the subduction matrix, then demonstrate that later IIIB/IIIC steps rebuild them from geometry rather than carrying stale state. Accepted-but-unsutured collision groups still need to populate the `CollisionPending` filter reason before remesh source selection.

Metrics: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIE4/phase-iii-slice-iiie4-metrics.jsonl`.
