# Phase III Pre-IIIE.6 zGamma Geophysics Profile

Verdict: PASS / IIIE.6 UNBLOCKED; ZGAMMA PAPER-FIDELITY HOLD REMAINS.

This checkpoint records the accepted zGamma authority decision before IIIE.6. The thesis and CGF paper still do not provide a closed-form `zGamma` ridge-profile law. CarrierLab therefore replaces the prior Earth-radius linear placeholder with a named geophysics-derived lab extension while keeping `bPaperFaithfulZGammaProfile = false` on generated records.

## Authority Decision

| Question | Decision | Evidence / rationale |
|---|---|---|
| Can the paper alone define a paper-faithful `zGamma` curve? | No | The local research checkpoint `phase-iii-pre-iiih0-zgamma-research.md` re-read thesis sections 3.3.2.1, 3.3.2.3, 3.3.3, the CGF 2019 paper, and appendix/table constants; all identify a generic ridge profile but no closed-form curve. |
| What parts remain paper-local? | Endpoints only | `z_e = -1 km` and `z_o = -6 km` remain the Table 3.2 / Appendix A constants. |
| What curve is used now? | Geophysics-derived square-root subsidence proxy | `zGamma(dGamma) = z_e + (z_o - z_e) * sqrt(clamp(dGamma / 3000 km, 0, 1))`. |
| Why `3000 km`? | Named lab reference distance | It sits inside the researched 70 Ma half-spreading distance range of roughly `1750-3500 km`; it is not a thesis constant. |
| Is this paper-faithful? | No | Generated records set `bUsedZGammaGeophysicsDerivedProfile = true` and `bPaperFaithfulZGammaProfile = false`. |
| Is alpha still single-use? | Yes | `alpha = dGamma / (dGamma + dPlate)` remains only the `zBar` / `zGamma` blend coefficient. `zGamma` depends on `dGamma`, not gap width. |

## Contract

| Contract surface | Current behavior | Gate |
|---|---|---|
| zGamma profile law | Square-root geophysics extension over `dGamma / 3000 km` | IIIE.4 fixture residuals and record hashes |
| Paper-fidelity label | Hold, not pass | IIIE.4 and IIIE.5 report rows expose `placeholder/geophysics/paper 0/1/0` or equivalent wording |
| Alpha decoupling | Same `dGamma` with different nearest-boundary width yields identical `zGamma` and profile `t` | Same-ridge width-invariance gate |
| Downstream preservation | IIIE.5 carries generated elevation, q1/q2/qGamma provenance, and zGamma hold flags through duplicate/re-index/re-compact | IIIE.5 provenance preservation gate |
| Forbidden overclaim | Reports may not say zGamma is paper-faithful while the geophysics flag is true and the paper flag is false | Claim audit and stop conditions |

## Gate Evidence

| Gate | Result | Evidence |
|---|---:|---|
| CarrierLabEditor build | pass | Incremental build succeeded after code and fixture updates. |
| CarrierLabPhaseIIIE4 | pass | `10/10` fixture records matched expected hashes; replay hash `32a28e51fcaf9160`; generated rows report `placeholder/geophysics/paper 0/1/0`. |
| Same-ridge zGamma width-invariance | pass | Ridge delta `0 km`, nearest-boundary delta `2.09e+03 km`, alpha delta `0.114`, zGamma delta `0`, profile-t delta `0`. |
| CarrierLabPhaseIIIE5 | pass | Topology/provenance gates passed; IIIE.4 provenance preservation hash `bf910c65a29bb27e`; zGamma hold carried with geophysics flag. |
| Paper-fidelity hold | hold | The profile is an accepted lab extension and remains ineligible for paper-fidelity consolidation without a paper-cited closed-form law. |

## Stop Conditions For IIIE.6+

- Stop if any generated divergent record uses the old Earth-radius linear placeholder.
- Stop if generated divergent records omit `bUsedZGammaGeophysicsDerivedProfile = true`.
- Stop if generated divergent records set `bPaperFaithfulZGammaProfile = true` for this geophysics-derived law.
- Stop if `zGamma` changes when `dGamma` is unchanged but nearest-boundary distance or alpha changes.
- Stop if IIIE.5 or later remesh records drop the geophysics-derived hold flags during topology rebuild or ledger wiring.
- Stop if reports claim the ridge-profile law is paper-faithful without a paper/thesis closed-form citation.

## Next Slice Boundary

IIIE.6 should wire selection, divergent field generation, topology rebuild, and process reset into a remesh-event ledger/reframe audit. It should also add the owed post-rebuild IIIB tracking gate. It should not add optimization, new q1/q2 policy, rifting, erosion, or long-horizon validation.
