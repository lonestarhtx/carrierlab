# Pre-IIIE.5 Thesis-Faithfulness Audit Of Divergent Ridge Generation

Audit of slice IIIE.4 (commit 14e4ab7) against thesis §3.3.2.1 ("Génération du
plancher océanique"). Scope: zΓ profile shape, double-use of α, the −1 km /
−6 km constants, and the structural independence of the IIIE.4 commandlet's
elevation oracle.

This audit is written. It does not modify code or run commandlets.

## Pre-IIIE.5.1 disposition

Pre-IIIE.5.1 preserves this audit as the problem statement, but supersedes the
original "defer concern #2 only in prose" disposition. The alpha double-use is
now closed in code: `alpha` remains only the thesis blend coefficient, while
`zGamma` uses a separate distance-profile parameter recorded as
`ZGammaProfileT`. The ridge-profile law remains a named lab placeholder with
`bPaperFaithfulZGammaProfile = false`; IIIE.5 may preserve it through topology
rebuild, but no consolidation may claim paper-faithful generated elevation
until a future slice replaces or justifies the profile law.

## Status

**GAP.** Two distinct gaps are identified, one critical:

1. **Oracle independence (concern 4) — CRITICAL.** The commandlet's
   `ComputeIndependentOracle` runs the same Lerp / blend / retangent formulas
   as the actor. The IIIE.4 report's `elevation residual = 0` is a tautology,
   not an independence signal. The gate as currently written does not
   exercise thesis-faithfulness.
2. **α double-use inside zΓ (concern 2) — DEVIATION.** The thesis defines α
   once, as the blend coefficient between z̄ and zΓ. The implementation also
   uses α as the parameter inside zΓ's Lerp from −1 km to −6 km. zΓ is
   written in the thesis as `zΓ(p, t)` — a function of position and time, not
   of α. The implementation's choice causes zΓ to depend on the gap geometry
   (plate-boundary distance dP) rather than on ridge distance (dΓ) alone.

Two other concerns resolve as follows:

3. **zΓ profile shape (concern 1) — PLACEHOLDER (acceptable subject to
   deferral language).** Thesis text on the shape is "un patron d'élévation
   représentant un profil générique de dorsale," with no closed form given.
   Figure 40 visually shows a non-linear curve, but this is illustrative.
   The implementation's choice of "linear in α" is implementor's-choice and
   can stand if explicitly deferred.
4. **Constants −1 km / −6 km (concern 3) — MATCH.** Table 3.2 (page 51,
   image 062) lists `z_e = -1 km` ("Élévation maximale des dorsales") and
   `z_o = -6 km` ("Élévation moyenne des plaines abyssales"). The
   implementation's `PhaseIIIE4RidgePeakElevationKm = -1.0` and
   `PhaseIIIE4AbyssalElevationKm = -6.0` are correct.

## Findings table

### 1. zΓ profile shape

| Aspect | Detail |
|---|---|
| Implementation | `Source/CarrierLab/Private/CarrierLabVisualizationActor.cpp:2427-2430`: `OutRecord.ZGammaElevation = FMath::Lerp(PhaseIIIE4RidgePeakElevationKm, PhaseIIIE4AbyssalElevationKm, OutRecord.Alpha);`. Linear interpolation from −1 km to −6 km parameterized by α. |
| Thesis text verbatim (cc5c6807-077.png, p. 66) | "L'élévation du point z(p, t+δt) dans la zone divergente est calculée par mélange continu entre l'interpolation linéaire des élévations des plaques z̄ et un patron d'élévation représentant un profil générique de dorsale zΓ (Figure 40): z(p, t+δt) = α z̄(p,t) + (1 − α) zΓ(p,t)" |
| Thesis on the shape itself | The text characterizes zΓ as "un patron d'élévation représentant un profil générique de dorsale" with no closed form. Figure 40 (cc5c6807-077.png) depicts zΓ as a smooth curved profile dropping from a ridge peak to abyssal depth — visually non-linear. |
| Verdict | **PLACEHOLDER.** Thesis text is silent on the closed form; implementation's "linear in α" is one defensible reading. Figure 40 hints at non-linear (sqrt or exp-style subsidence in the geophysics literature), but the thesis text does not specify, so this is implementor's-choice rather than a GAP. Must be paired with an explicit deferral, since the parameterization is also entangled with concern #2. |

### 2. α double-use as both blend coefficient and zΓ parameter

| Aspect | Detail |
|---|---|
| Implementation | `Source/CarrierLab/Private/CarrierLabVisualizationActor.cpp:2424-2433`: `OutRecord.Alpha = ... RidgeDistanceKm / (RidgeDistanceKm + NearestBoundaryDistanceKm) ...`; then `OutRecord.ZGammaElevation = FMath::Lerp(-1.0, -6.0, OutRecord.Alpha);`; then `OutRecord.Elevation = Alpha * ZBar + (1 − Alpha) * ZGamma`. The same α drives both the zΓ shape AND the blend. |
| Thesis text verbatim (cc5c6807-077.png, p. 66) | "Soit dΓ(p) et dP(p) les distances depuis p respectivement à la dorsale et à la plaque divergente la plus proche (Figure 39). Nous définissons le facteur interpolant α = dΓ(p)/(dΓ(p) + dP(p)). L'élévation du point z(p, t+δt) dans la zone divergente est calculée par mélange continu entre l'interpolation linéaire des élévations des plaques z̄ et un patron d'élévation représentant un profil générique de dorsale zΓ (Figure 40): z(p, t+δt) = α z̄(p,t) + (1 − α) zΓ(p,t)" |
| Thesis treatment of zΓ | zΓ is written `zΓ(p, t)` — function of position (and time). It is never written `zΓ(α)` or in any way parameterized by α. α has exactly one role in the thesis: the blend coefficient. |
| Concrete consequence of the implementation choice | Two samples at the same dΓ but different dP get different zΓ values. Sample A (dΓ=100 km, dP=100 km, α=0.5) → zΓ=−3.5 km. Sample B (dΓ=100 km, dP=200 km, α=0.333) → zΓ≈−2.67 km. Same distance from the ridge, different "generic ridge profile" depths. The ridge-to-abyssal subsidence rate per km of ridge distance varies with gap width. |
| Verdict | **GAP / DEVIATION.** Thesis prescribes one role for α (blend). Implementation invents a second role (zΓ parameter). The thesis's intent is that zΓ is positional, dependent on ridge distance dΓ alone (or absolute position). The implementation's choice couples zΓ to plate-boundary distance dP, which is not in the thesis. Should be flagged in the audit deviations even if elevation arithmetic at the gap interior happens to land in a plausible range. |

### 3. Constants −1 km and −6 km

| Aspect | Detail |
|---|---|
| Implementation | `Source/CarrierLab/Private/CarrierLabVisualizationActor.cpp:42-43`: `constexpr double PhaseIIIE4RidgePeakElevationKm = -1.0;` and `constexpr double PhaseIIIE4AbyssalElevationKm = -6.0;`. Mirrored at `Source/CarrierLab/Private/CarrierLabPhaseIIIE4Commandlet.cpp:17-18`: `constexpr double RidgePeakElevationKm = -1.0; constexpr double AbyssalElevationKm = -6.0;`. |
| Thesis text verbatim (cc5c6807-062.png, p. 51, Table 3.2) | Symbol `z_e`, Description "Élévation maximale des dorsales", Valeur "−1 km". Symbol `z_o`, Description "Élévation moyenne des plaines abyssales", Valeur "−6 km". Caption: "Constantes utilisées dans le modèle de génération tectonique. Les valeurs indiquées ont été utilisées pour générer tous les exemples montrés dans le manuscrit." |
| Verdict | **MATCH.** Constants are exactly z_e and z_o from Table 3.2 of the thesis. The IIIE.4 report's claim that these come from Table 3.2 is verified. |

### 4. Oracle independence (commandlet vs actor)

| Aspect | Detail |
|---|---|
| Actor implementation (`Source/CarrierLab/Private/CarrierLabVisualizationActor.cpp:2413-2433`) | Computes ZBar=Lerp(Q1Elev,Q2Elev, Q1Dist/(Q1Dist+Q2Dist)); Alpha=RidgeDist/(RidgeDist+NearestBoundaryDist); ZGamma=Lerp(−1km,−6km,Alpha); Elevation=Alpha·ZBar+(1−Alpha)·ZGamma; RidgeDirection=retangent((Sample−QGamma)×Sample). |
| Commandlet "independent" oracle (`Source/CarrierLab/Private/CarrierLabPhaseIIIE4Commandlet.cpp:282-308`, `ComputeIndependentOracle`) | Computes OutZBar=Lerp(Q1Elev,Q2Elev, Q1Dist/(Q1Dist+Q2Dist)); OutAlpha=RidgeDist/(RidgeDist+NearestBoundaryDist); OutZGamma=Lerp(−1km,−6km,OutAlpha); OutElevation=OutAlpha·OutZBar+(1−OutAlpha)·OutZGamma; OutRidgeDirection=retangent((Sample−QGamma)×Sample). |
| Mathematical relation | Identical formulas. Identical constants. Identical operand structure. The same Sample, Q1, Q2, ExpectedQ1Elevation, ExpectedQ2Elevation flow into both, with QGamma computed identically on both sides as `NormalizeOrFallback(Q1+Q2, Sample)`. |
| Why "elevation residual = 0" | Mathematically guaranteed by structural identity. Same inputs into bit-exact same arithmetic must produce bit-exact same outputs (modulo IEEE rounding, which is also identical). The residual cannot be anything other than zero short of compiler bugs. |
| Pattern recognition | Matches the IIIC.4 circular-oracle anti-pattern (oracle reads/derives implementation outputs and re-applies the same formula, producing bit-exact zero residual mathematically). The "independent oracle" performs no independent verification of the thesis specification; it verifies only that the implementation is internally self-consistent — i.e., that the same code produces the same result twice. |
| Verdict | **GAP — CRITICAL.** The "independent oracle" is not independent. The IIIE.4 elevation/alpha/zBar/zGamma residual gates are tautologies. They prove nothing about thesis-faithfulness. The IIIE.4 report's PASS verdict on these residual gates is a single-source confirmation, not multi-source independent agreement. The IIIE consolidation regression token built on this signal would inherit the same single-source weakness. |

## Recommended dispositions

### Concern 4 — oracle independence (must close before IIIE consolidation)

The commandlet's `ComputeIndependentOracle` must be replaced with a structurally
independent computation. Acceptable forms:

- **Closed-form per fixture.** For each fixture, derive expected ZBar, Alpha,
  ZGamma, Elevation algebraically from sample/Q1/Q2 unit positions, write
  these expected values directly into the `FFixtureSpec`, and check the
  actor's output against these constants. The expected values come from a
  geometric derivation done at fixture-design time, not from rerunning the
  same Lerp at runtime.
- **Different parameterization.** Compute distances/elevations using a
  different but mathematically equivalent path (e.g., spherical law-of-cosines
  for distances vs the Acos(Dot) form the actor uses; explicit linear-blend
  expansion vs FMath::Lerp; a different ZGamma reference profile entirely
  for the residual check). The point is that two formulas must independently
  derive the same number — bit-exact agreement then means something.
- **Dual-implementation cross-check.** A second formulation in pure
  arithmetic (e.g., `(1-α)·z_e + α·z_o` written out instead of `Lerp(-1,-6,α)`)
  is the weakest acceptable form, since the algebraic identity is a one-line
  proof. Better: change the operand path entirely.

The fixture residuals are not the only signal — the record hash (which mixes
in many fields) is broader. But the oracle cited in the report's "elevation
residual = 0" gate must give an independent signal, not a tautology.

### Concern 2 — α double-use (must close before IIIE consolidation, OR explicitly defer with user approval)

Two acceptable resolutions:

**Option A (paper-aligned).** Decouple zΓ from α. Parameterize zΓ by ridge
distance dΓ alone (or a normalized form). The simplest such form is
`zΓ(p) = Lerp(z_e, z_o, clamp(dΓ / d_ref, 0, 1))` with `d_ref` a fixed
reference distance (Table 3.2's `r_c = 4200 km` is a defensible choice for
the continental-collision-distance scale, but a ridge-specific scale would
be more honest). This makes zΓ depend on ridge distance only, matching the
thesis's `zΓ(p, t)` notation. The blend then has α as its sole role.

**Option B (deferral with explicit approval).** Keep the current "α inside zΓ
also" choice as a named placeholder, but only after explicit user-approved
deferral language is added to the IIIE.4 report and to a forward-looking
TODO. The deferral must name that:

- "zΓ is currently parameterized by the same α used in the blend, which
  couples its profile shape to plate-boundary distance dP. The thesis
  defines zΓ as `zΓ(p, t)` — a function of position only — and assigns α
  exactly one role: the blend coefficient. The IIIE.4 implementation's
  reuse of α inside zΓ is a lab simplification, not paper-faithful, and is
  to be replaced when a thesis-aligned zΓ profile (Option A above, or a
  sqrt-of-distance subsidence law extracted from external geophysics
  literature) is implemented."

Pick A or B before consolidation. Both must close concern #4 first, since
without an independent oracle there is no way to verify the chosen
disposition is implemented correctly.

### Concern 1 — zΓ profile shape (deferrable as named placeholder)

The thesis is silent on the closed form. A linear ramp is a defensible
placeholder, particularly under Option A above where the linear is in dΓ
(monotone subsidence with ridge distance) rather than in α. If concern #2
resolves to Option A, concern #1 is acceptably-deferred.

If concern #2 resolves to Option B, then concern #1's deferral language
must also explicitly disclose that the shape is linear and not the
sqrt-of-distance subsidence figure-40 visually depicts.

### Concern 3 — constants (no action)

`PhaseIIIE4RidgePeakElevationKm = -1.0` and `PhaseIIIE4AbyssalElevationKm = -6.0`
are correct from Table 3.2. The IIIE.4 report's citation is verified.

## Open questions

1. **Ridge direction `q` vs `qΓ`.** Thesis (cc5c6807-077.png, p. 66): "Soit
   q la projection de p sur la dorsale, nous définissons: r(p) = (p − q) × p."
   q is the *projection of p onto the ridge* — sample-dependent, varies
   with p. The implementation uses qΓ (spherical midpoint of q1 and q2)
   instead: `RetangentAndNormalizeVectorField(CrossProduct(SamplePosition − QGammaUnitPosition, SamplePosition), SamplePosition)`.
   For straight ridges with parallel boundaries qΓ ≈ projection-of-p, but in
   general qΓ is the midpoint of the nearest boundary points to p, not p's
   own projection onto the ridge. For divergent gaps with curved boundaries
   or non-parallel q1/q2 directions, these differ. The thesis text does not
   say "use qΓ for r(p)." This is plausibly small but unverified, and
   propagates into the same oracle (which uses qΓ both sides). Needs a
   geometric residual fixture: place p far from the q1/q2 perpendicular and
   compare the two definitions.

2. **z̄ interpolation parameter.** Thesis text writes "interpolation linéaire
   des élévations des plaques z̄" — linear interpolation of plate elevations,
   with no explicit parameter formula. The implementation parameterizes by
   `Q1DistanceKm / (Q1DistanceKm + Q2DistanceKm)` (proportional to geodesic
   distance to each boundary). Figure 40 shows z̄ as a straight line between
   the two plate boundary elevations, which is consistent with the geodesic
   parameterization. Probably MATCH but not strictly verified — the thesis
   could equally support spherical-arc-length parameterization or
   elevation-fraction parameterization. Worth a one-line confirmation if
   §3.3.2.3 (the implementation section) names the formula explicitly.

3. **qΓ definition.** Thesis says (cc5c6807-079.png, p. 68, paraphrased):
   "qΓ = R · (qi + qj) / |qi + qj|" — sphere-projected midpoint of q1 and q2.
   The actor's `BoundaryAudit.QGammaUnitPosition` is provided by IIIE.2;
   verifying §3.3.2.3 vs IIIE.2 is out of this audit's scope but should be
   confirmed before any IIIE consolidation regression-token claim.

4. **The thesis is silent on the closed form of zΓ.** The geophysics
   literature (Parsons & Sclater 1977 and successors) gives a sqrt-of-time
   /sqrt-of-distance subsidence law for oceanic crust. If a future IIIE
   slice intends to replace the linear placeholder with a thesis-aligned
   form, the natural reference is geophysics, not the thesis. This is a
   scope question for the user, not a thesis-faithfulness gap.

## Recommendation

**Block IIIE.5 until concern #4 (oracle independence) is closed.** The current
elevation/alpha/zBar/zGamma residual gates carry no thesis-faithfulness
signal, so any IIIE consolidation token built on top of IIIE.4 inherits a
tautological foundation. Concern #2 (α double-use) should also be resolved
or formally deferred before consolidation, but its resolution depends on a
working independent oracle to verify whichever disposition is chosen.
Concern #3 is fine as-is; concern #1 deferral language travels with concern
#2's resolution. IIIE.5 may proceed once concern #4 is closed and concern #2
has either been corrected (Option A) or explicitly deferred with the named
language (Option B).
