# Phase IIIE.4 Divergent Oceanic Field Generation

Verdict: PASS / IIIE.5 TOPOLOGY UNBLOCKED; ZGAMMA PAPER-FIDELITY HOLD. This slice generates audit-only oceanic fields for IIIE.3 divergent gap routes. It does not rebuild topology, mutate the global remesh TDS, reset process state, optimize replay, or resolve multi-hit samples.

## Scope

- IIIE.4 consumes only `NoHitDivergentGap` and `DivergentGapAfterFiltering` records from IIIE.3.
- It obtains q1/q2/qGamma from the IIIE.2 continuous boundary query and records q1/q2 plate ids, edge ids, qGamma, signed separating velocity, generated elevation, age, and ridge direction.
- Generated divergent crust has `OceanicAge = 0` and `RidgeDirection = retangent(normalize((p - qGamma) x p))`.
- Elevation follows the local extraction contract `z = alpha * zBar(p) + (1 - alpha) * zGamma(p)`, with `alpha = dGamma / (dGamma + dPlate)`.
- `zBar(p)` is compared against fixture-owned expected constants for distance interpolation between q1/q2 boundary elevations.
- Pre-IIIE.5.1 closes the prior alpha double-use deviation: `alpha` is no longer used inside `zGamma`. The current `zGamma` is a separate distance-profile placeholder parameterized by `dGamma / EarthRadiusKm` and anchored to thesis Table 3.2 constants: ridge peak `-1 km`, abyssal plain `-6 km`.
- `zGamma` remains a paper-fidelity hold. Thesis section 3.3.2.1 names a generic ridge profile and section 3.3.2.3 defines qGamma, but the local extraction has no closed-form zGamma curve. IIIE.5 may preserve this field through topology rebuild, but no consolidation may claim the ridge-profile law is paper-faithful until a future slice replaces or justifies it.
- Resolved single-hit and unresolved multi-hit classes are rejected before field generation. Non-positive signed separating velocity is an anomaly, not a fallback.

## Gates

| Gate | Result | Evidence |
|---|---:|---|
| no-hit divergent oceanic generation | pass | class `no-hit divergent gap`, generated `1`, boundary `1`, anomaly `0`, rejected `0/0`, q1/q2 `0/1`, assigned `0`, signed velocity `0.002`, q dist residuals `1.82e-12/1.82e-12 km`, ridge/nearest residuals `4.09e-12/1.82e-12 km`, qGamma residual `0`, alpha/elev residuals `6.11e-16/3.11e-15`, zGamma profile residuals `4.09e-12 km/0 km/6.94e-16`, placeholder/paper `1/0`, ridge residual `0`, radial dot `0`, hash `0532a906502dd2f6`, expected `0532a906502dd2f6`, match `1`. |
| filter-exhausted divergent oceanic generation | pass | class `divergent gap after filtering`, generated `1`, boundary `1`, anomaly `0`, rejected `0/0`, q1/q2 `0/1`, assigned `0`, signed velocity `0.002`, q dist residuals `1.82e-12/1.82e-12 km`, ridge/nearest residuals `4.09e-12/1.82e-12 km`, qGamma residual `0`, alpha/elev residuals `6.11e-16/3.11e-15`, zGamma profile residuals `4.09e-12 km/0 km/6.94e-16`, placeholder/paper `1/0`, ridge residual `0`, radial dot `0`, hash `c86fa48aad744c88`, expected `c86fa48aad744c88`, match `1`. |
| non-separating q1/q2 anomaly | pass | class `no-hit divergent gap`, generated `0`, boundary `1`, anomaly `1`, rejected `0/0`, q1/q2 `0/1`, assigned `-1`, signed velocity `-0.002`, q dist residuals `1.82e-12/1.82e-12 km`, ridge/nearest residuals `0/0 km`, qGamma residual `0`, alpha/elev residuals `0/0`, zGamma profile residuals `0 km/0 km/0`, placeholder/paper `0/0`, ridge residual `0`, radial dot `0`, hash `f6b5b75d253f9abe`, expected `f6b5b75d253f9abe`, match `1`. |
| resolved single-hit route rejected | pass | class `resolved single hit`, generated `0`, boundary `0`, anomaly `0`, rejected `1/0`, q1/q2 `-1/-1`, assigned `-1`, signed velocity `0.002`, q dist residuals `0/0 km`, ridge/nearest residuals `0/0 km`, qGamma residual `0`, alpha/elev residuals `0/0`, zGamma profile residuals `0 km/0 km/0`, placeholder/paper `0/0`, ridge residual `0`, radial dot `0`, hash `9eecee807567c02a`, expected `9eecee807567c02a`, match `1`. |
| unresolved multi-hit route rejected | pass | class `unresolved mixed-material multi-hit`, generated `0`, boundary `0`, anomaly `0`, rejected `1/1`, q1/q2 `-1/-1`, assigned `-1`, signed velocity `0.002`, q dist residuals `0/0 km`, ridge/nearest residuals `0/0 km`, qGamma residual `0`, alpha/elev residuals `0/0`, zGamma profile residuals `0 km/0 km/0`, placeholder/paper `0/0`, ridge residual `0`, radial dot `0`, hash `c239c40acdaa3450`, expected `c239c40acdaa3450`, match `1`. |
| no two-plate boundary pair anomaly | pass | class `no-hit divergent gap`, generated `0`, boundary `0`, anomaly `0`, rejected `0/0`, q1/q2 `-1/-1`, assigned `-1`, signed velocity `0.002`, q dist residuals `0/0 km`, ridge/nearest residuals `0/0 km`, qGamma residual `0`, alpha/elev residuals `0/0`, zGamma profile residuals `0 km/0 km/0`, placeholder/paper `0/0`, ridge residual `0`, radial dot `0`, hash `a4cee3c7316f545a`, expected `a4cee3c7316f545a`, match `1`. |
| asymmetric q1/q2 elevation interpolation | pass | class `no-hit divergent gap`, generated `1`, boundary `1`, anomaly `0`, rejected `0/0`, q1/q2 `1/0`, assigned `1`, signed velocity `0.002`, q dist residuals `1.14e-12/2.27e-12 km`, ridge/nearest residuals `0/1.14e-12 km`, qGamma residual `5.55e-17`, alpha/elev residuals `1.11e-16/0`, zGamma profile residuals `0 km/0 km/2.78e-17`, placeholder/paper `1/0`, ridge residual `2.49e-16`, radial dot `0`, hash `253f98e95432a3dc`, expected `253f98e95432a3dc`, match `1`. |
| off-axis sample ridge direction | pass | class `no-hit divergent gap`, generated `1`, boundary `1`, anomaly `0`, rejected `0/0`, q1/q2 `1/0`, assigned `1`, signed velocity `0.002`, q dist residuals `4.09e-12/1.82e-12 km`, ridge/nearest residuals `0/4.09e-12 km`, qGamma residual `0`, alpha/elev residuals `5.55e-16/0`, zGamma profile residuals `0 km/0 km/0`, placeholder/paper `1/0`, ridge residual `0`, radial dot `0`, hash `5635bde78bb5f70d`, expected `5635bde78bb5f70d`, match `1`. |
| antipodal qGamma degenerate ridge direction | pass | class `no-hit divergent gap`, generated `1`, boundary `1`, anomaly `0`, rejected `0/0`, q1/q2 `0/1`, assigned `0`, signed velocity `0.002`, q dist residuals `1.82e-12/0 km`, ridge/nearest residuals `0/1.82e-12 km`, qGamma residual `0`, alpha/elev residuals `0/0`, zGamma profile residuals `0 km/0 km/0`, placeholder/paper `1/0`, ridge residual `0`, radial dot `0`, hash `4ee4002b0dbccfd6`, expected `4ee4002b0dbccfd6`, match `1`. |
| same-ridge different-gap-width zGamma | pass | class `no-hit divergent gap`, generated `1`, boundary `1`, anomaly `0`, rejected `0/0`, q1/q2 `0/1`, assigned `0`, signed velocity `0.002`, q dist residuals `1.82e-12/1.82e-12 km`, ridge/nearest residuals `4.09e-12/1.82e-12 km`, qGamma residual `0`, alpha/elev residuals `5e-16/3.55e-15`, zGamma profile residuals `4.09e-12 km/0 km/6.94e-16`, placeholder/paper `1/0`, ridge residual `0`, radial dot `0`, hash `2db178e0972a0f48`, expected `2db178e0972a0f48`, match `1`. |
| Same-ridge zGamma width-invariance | pass | ridge delta `0 km`, nearest-boundary delta `2.09e+03 km`, alpha delta `0.114`, zGamma delta `0`, profile-t delta `0`. Same dGamma with different gap width must keep zGamma fixed while alpha changes. |
| Fixture record-hash regression | pass | `10/10` fixture records matched their expected hashes. |
| Same-seed oceanic-generation replay | pass | Replay hashes `0532a906502dd2f6` and `0532a906502dd2f6`. |
| zGamma paper-fidelity hold | hold | Generated records intentionally report `bUsedZGammaDistanceProfilePlaceholder=1` and `bPaperFaithfulZGammaProfile=0`; this is a topology-unblocking placeholder, not a paper-faithful ridge-profile claim. |

## Contract Table

| Paper / IIIE.1 requirement | CarrierLab support now | IIIE obligation still ahead | Gate needed |
|---|---|---|---|
| Zero valid hits become divergent gap fill | IIIE.4 accepts only no-hit and filter-exhausted route classes | Wire these records into the actual remesh event in IIIE.5 | End-to-end remesh event fixture with zero-hit route |
| q1/q2 are continuous nearest boundary points on different plates | IIIE.4 calls the IIIE.2 query and records q ids, edges, qGamma, distances, and elevations | Preserve this provenance when topology rebuild duplicates/re-indexes samples | Event-log row carries q1/q2/qGamma through rebuild |
| New oceanic crust age is zero | Generation records `OceanicAge = 0` and gates residual against fixture-owned expected constants | Mutate global samples only inside the remesh event | Generated sample field residuals and record-hash regression |
| Ridge direction is `(p - qGamma) x p` retangented/normalized | Generated records have non-zero tangent ridge vectors with near-zero radial dot | Preserve vector fields through duplicate/re-index/re-compact | Vector magnitude and radial-dot oracle |
| zGamma is a ridge profile, not a second use of alpha | Pre-IIIE.5.1 records separate `ZGammaProfileT = dGamma / EarthRadiusKm` and gates same-ridge/different-gap invariance | Future slice must replace or justify the profile law before paper-fidelity consolidation | Paper-cited zGamma law or explicit lab-policy acceptance gate |
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
- Stop if topology rebuild claims `zGamma` or generated elevation is fully paper-faithful while generated records still report `bPaperFaithfulZGammaProfile = false`.
- Stop if `zGamma` changes when `dGamma` is unchanged but gap width / nearest-boundary distance changes.
- Stop if a non-positive q1/q2 separating velocity generates oceanic fields.
- Stop if a missing two-plate boundary pair fabricates q1/q2, plate ownership, or elevation.
- Stop if Stage 1.5 prior-owner, endpoint/midpoint, recovery, or anchoring policy becomes authority in the primary IIIE remesh path.
- Stop if IIIE.5 topology rebuild drops q1/q2/qGamma, signed velocity, age, elevation, or ridge-direction evidence from the event log.

## Next Slice Boundary

IIIE.5 should rebuild plate-local topology from the global TDS assignment: duplicate, re-index, and re-compact per plate while preserving motion and the IIIE.4 generated field records. It must also perform the remesh process-state reset contracted in IIIE.1: invalidate subduction marks, active convergence lists, distance-to-front records, and the subduction matrix, then demonstrate that later IIIB/IIIC steps rebuild them from geometry rather than carrying stale state. Accepted-but-unsutured collision groups still need to populate the `CollisionPending` filter reason before remesh source selection.

Metrics: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIE4/phase-iii-slice-iiie4-metrics.jsonl`.
