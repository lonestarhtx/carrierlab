# Phase IIIE.2 Continuous q1/q2/qGamma Contract

Verdict: PASS / IIIE.3 UNBLOCKED. This slice implements the paper-faithful continuous divergent-gap provenance query only. It does not implement remesh source filtering, gap-field mutation, topology rebuild, process-state reset, optimization, or oceanic generation.

## Scope

- Primary IIIE gap-fill provenance now has a continuous closest-point-on-boundary-edge query surface for q1 and q2, with q2 required to come from a different plate than q1.
- qGamma is computed as the normalized spherical midpoint provenance `normalize(q1 + q2)`.
- Degenerate qGamma input is observable through `QGammaInputNorm`; the query falls back to the sample direction only when q1 + q2 has zero usable length.
- Exact equal-distance q1 ties use deterministic `(plate id, edge id)` ordering as a named lab convention for stable diagnostics; this is not an ownership or remesh-winner rule.
- Endpoint/midpoint boundary sampling remains diagnostic-only in this checkpoint; it is not used by the primary IIIE.2 query.
- The Stage 1.5 resampling path remains lab-policy code and is not promoted into primary Phase IIIE remesh.

- IIIB independent-signature regression is intentionally absent here because IIIE.2 is state-free provenance math; it returns in IIIE.3 when the slice consumes plate-local BVHs and process state.

## Gates

| Gate | Result | Evidence |
|---|---:|---|
| interior continuous edge projection | pass | q1 plate/edge `0/0`, q2 plate/edge `1/1`, q1 residual `1.14e-16`, q2 residual `2.48e-16`, qGamma residual `2.48e-16`, qGamma input norm `1.41`, qGamma unit residual `1.11e-16`, hash `3de8ed4da74d8820`. |
| arc endpoint clipping | pass | q1 plate/edge `0/0`, q2 plate/edge `1/1`, q1 residual `5.55e-17`, q2 residual `2.48e-16`, qGamma residual `1.11e-16`, qGamma input norm `1.53`, qGamma unit residual `0`, hash `50a9561d0a96a0b1`. |
| different-plate q2 selection | pass | q1 plate/edge `0/0`, q2 plate/edge `1/2`, q1 residual `1.14e-16`, q2 residual `1.57e-16`, qGamma residual `0`, qGamma input norm `1.64`, qGamma unit residual `0`, hash `1eeed119f28d977f`. |
| degenerate qGamma fallback is observable | pass | q1 plate/edge `0/0`, q2 plate/edge `1/1`, q1 residual `0`, q2 residual `0`, qGamma residual `0`, qGamma input norm `1.22e-16`, qGamma unit residual `0`, hash `985d6a11f669d872`. |
| deterministic equal-distance tie convention | pass | q1 plate/edge `0/1`, q2 plate/edge `1/0`, q1 residual `5.55e-17`, q2 residual `5.55e-17`, qGamma residual `5.55e-17`, qGamma input norm `2`, qGamma unit residual `0`, hash `16fc6b3b0ea346fb`. |
| no two-plate boundary anomaly | pass | q1 plate/edge `-1/-1`, q2 plate/edge `-1/-1`, q1 residual `0`, q2 residual `0`, qGamma residual `0`, qGamma input norm `0`, qGamma unit residual `0`, hash `7907026162c72402`. |
| Actor-state boundary path | pass | built `316` current-state boundary edges; explicit hash `1b4c1d1b3623535e`, state hash `1b4c1d1b3623535e`, q1/q2/qGamma residuals `0` / `0` / `0`. |
| Same-seed provenance replay | pass | Replay hashes `3de8ed4da74d8820` and `3de8ed4da74d8820`. |

## Diagnostic Approximation Check

| Fixture | Continuous q1 distance km | Endpoint/midpoint diagnostic extra distance km | Interpretation |
|---|---:|---:|---|
| interior continuous edge projection | `1111.949266446` | `456.571290353` | Diagnostic only; the primary query uses the continuous edge point. |
| arc endpoint clipping | `2223.898532891` | `0.000000000` | Diagnostic only; the primary query uses the continuous edge point. |
| different-plate q2 selection | `889.559413156` | `0.000000000` | Diagnostic only; the primary query uses the continuous edge point. |
| degenerate qGamma fallback is observable | `0.000000000` | `0.000000000` | Diagnostic only; the primary query uses the continuous edge point. |
| deterministic equal-distance tie convention | `2223.898532891` | `0.000000000` | Diagnostic only; the primary query uses the continuous edge point. |

## Stop Conditions

- Hold IIIE.3 if any primary query result uses an endpoint/midpoint approximation as authority.
- Hold IIIE.3 if q2 can come from the same plate as q1.
- Hold IIIE.3 if zero two-plate boundary candidates trigger any IIIE.1-forbidden remesh winner, recovery, or ownership-retention policy.
- Hold IIIE.3 if qGamma is not a normalized spherical midpoint of q1 and q2.

## Next Slice Boundary

IIIE.3 should wire this query into filtered remesh source selection: center-out rays, process-state triangle invisibility, unresolved multi-hit gating, and zero-hit classification. It should still leave topology rebuild and process-state reset to later IIIE slices unless explicitly reprioritized.

Metrics: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIE2/phase-iii-slice-iiie2-metrics.jsonl`.
