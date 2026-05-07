# Pre-IIIE.5.1 zGamma Contract Hardening

Verdict: PASS / IIIE.5 topology work is unblocked, with a continuing
zGamma paper-fidelity hold. This hardening slice closes the prior alpha
double-use deviation before IIIE.5 starts mutating topology. It does not
implement topology rebuild, remesh cadence, process-state reset, q1/q2
selection, or a paper-cited ridge-profile law.

## Contract Change

| Surface | Before | Pre-IIIE.5.1 status | IIIE.5 obligation |
|---|---|---|---|
| `alpha` | Used both as the thesis blend coefficient and as the parameter inside `zGamma` | Used only as `dGamma / (dGamma + dPlate)` for `z = alpha * zBar + (1 - alpha) * zGamma` | Preserve `alpha` as a blend coefficient only |
| `zGamma` parameter | Coupled to gap width because it reused `alpha` | Uses a separate recorded distance profile: `ZGammaProfileT = dGamma / EarthRadiusKm` | Preserve fields through duplicate/re-index/re-compact without claiming paper fidelity |
| Paper-fidelity status | Deferred in prose | Runtime records expose `bUsedZGammaDistanceProfilePlaceholder = true` and `bPaperFaithfulZGammaProfile = false` | Keep this as a stop condition until a future paper-cited or approved lab-policy profile lands |
| Regression gate | 9 fixture hashes after Pre-IIIE.5 oracle hardening | 10 fixture hashes, adding same-ridge/different-gap-width zGamma invariance | Do not route IIIE.5 topology evidence through stale IIIE.4 hashes |

## Gate Evidence

| Gate | Result | Evidence |
|---|---:|---|
| CarrierLabEditor build | pass | `Run-CarrierLabEditorBuild.ps1` succeeded after the code change and again after burning new hashes. |
| IIIE.4 commandlet | pass | `CarrierLabPhaseIIIE4` returned 0 and regenerated `docs/checkpoints/phase-iii-slice-iiie4-report.md`. |
| Fixture record-hash regression | pass | `10/10` fixture records matched expected hashes. |
| Same-ridge zGamma width-invariance | pass | Same `dGamma` with different nearest-boundary distance produced alpha delta `0.114`, zGamma delta `0`, and profile-t delta `0`. |
| Paper-fidelity hold visibility | hold | Generated records report placeholder/paper `1/0`; this is intentional and blocks any full paper-faithful elevation claim. |

## What This Resolves

- Concern #2 from `phase-iii-pre-iiie5-zgamma-audit.md`: alpha is no longer
  reused inside `zGamma`.
- The test-token ratchet risk before IIIE.5: downstream topology tests will now
  encode a separated placeholder contract rather than the alpha double-use.
- The audit visibility risk: the placeholder is visible in records, JSONL,
  hash content, and the IIIE.4 report, not only in prose.

## What Remains Deferred

The ridge-profile law itself is still not paper-faithful. Thesis section
3.3.2.1 names a generic ridge profile and section 3.3.2.3 defines qGamma, but
the local extraction still does not provide a closed-form profile curve.
Pre-IIIE.5.1 chooses `EarthRadiusKm` as a transparent lab reference distance
only to decouple the profile from gap width. This is not a thesis citation.

## Stop Conditions For IIIE.5+

- Stop if IIIE.5 or later reports claim generated elevation is fully
  paper-faithful while generated records still carry
  `bPaperFaithfulZGammaProfile = false`.
- Stop if any topology rebuild test assumes zGamma may vary with gap width
  when `dGamma` is unchanged.
- Stop if the new zGamma fields are dropped during duplicate/re-index/re-compact
  event logging.
- Stop if unresolved multi-hit samples, missing two-plate boundary pairs, or
  non-separating q1/q2 kinematics are routed to oceanic generation.

## Next Slice Boundary

IIIE.5 may proceed to topology rebuild and process-state reset. It should carry
the IIIE.4 oceanic fields, q1/q2/qGamma provenance, signed separation velocity,
and zGamma placeholder flags through the remesh event record. It should not
replace the ridge-profile law unless IIIE.5 is explicitly rescoped.
