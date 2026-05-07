# Pre-IIIE.5 Oracle Independence + Concern #2 zΓ Disposition Design

Companion to `docs/checkpoints/phase-iii-pre-iiie5-zgamma-audit.md`. The audit
identified a CRITICAL oracle gap (concern #4) and a deviation in zΓ
parameterization (concern #2). This design specifies the bounded slice that
closes both, in one Codex-executable commit.

This document is design only. No code or commandlet runs.

## Status

**Design ready.** Incorporates three refinements from adversarial review:
(a) zΓ deferral language now names "linear approximation of figure 40's
curve" explicitly so future reviewers do not read figure-40 consistency
as stronger than it is; (b) an active hash-regression gate burns the six
current report hashes into `FFixtureSpec.ExpectedRecordHash` instead of
relying on a passive observation; (c) Codex precision guidance pinned to
`'%.17g'`/`repr(x)` (≥17 significant digits) since IEEE-754 double's
~15.95-decimal-digit precision means "14+" is below round-trip safety.

- Task 1 (oracle independence): closed-form-per-fixture (Technique A) for all
  scalar gates; property-invariant gates for RidgeDirection magnitude and
  radial-dot; hand-derived expected unit vector for direction; active
  record-hash regression gate (string-equality match). New fixture types
  add asymmetric geometry to break symmetry-only-passing oracles.
- Task 2 (concern #2 disposition): **Option B (named placeholder with
  explicit deferral language).** A re-read of §3.3.2.3 (page 68) and §3.3.3
  (pages 69–70) showed that Option A's "fixed-slope ridge profile with chosen
  reference distance" is no more thesis-faithful than the current
  implementation; it just trades one non-thesis choice for another. Both
  Option A and Option B are LINEAR approximations of figure 40's curved
  profile; neither captures curvature. Option B preserves the audit's
  "deviation flagged" without over-promising thesis fidelity.

## Task 1 — Oracle independence design

### Problem recap

Current commandlet `ComputeIndependentOracle` (`Source/CarrierLab/Private/CarrierLabPhaseIIIE4Commandlet.cpp:282-308`)
re-computes ZBar, Alpha, ZGamma, Elevation, RidgeDirection by running the
same Lerp/blend/retangent expressions as the actor's
`PopulatePhaseIIIE4OceanicRecord` (`Source/CarrierLab/Private/CarrierLabVisualizationActor.cpp:2369-2443`).
Residual = 0 is mathematically forced; the gate carries no thesis-faithfulness
signal.

### Strategy

Switch the oracle to **Technique A (closed-form-per-fixture)** for every
scalar gate. Expected values are derived ONCE at fixture-design time using
a different operand path (haversine / explicit cos·cos+sin·sin for distance,
explicit `(1-α)·a + α·b` for blends), the result rounded and burned into
`FFixtureSpec` as constants. The runtime oracle just reads those constants
and compares the actor's runtime output against them.

This is the strongest available form: the runtime oracle never re-applies the
actor's arithmetic. To produce the same number twice the actor must agree
with an algebraically-derived constant, which is the actual independence
signal.

For RidgeDirection, supplement Technique A (expected unit vector) with two
property-invariant checks: magnitude = 1 ± ε and radial dot = 0 ± ε. These
are properties of *any* tangent unit vector at p, regardless of formula —
they bound the gate even if a future fixture lacks an analytic expected
vector.

### Per-gate design table

| Gate | Independence technique | Expected-value derivation | Tolerance |
|---|---|---|---|
| `Q1DistanceKm` | A — closed-form-per-fixture | Hand: `R · 2·asin(sqrt(hav(Δlat) + cos(lat₁)·cos(lat₂)·hav(Δlon)))` (haversine). Burned into `FFixtureSpec.ExpectedQ1DistanceKm`. | 1×10⁻⁶ km |
| `Q2DistanceKm` | A | Same formula, different endpoints. `FFixtureSpec.ExpectedQ2DistanceKm`. | 1×10⁻⁶ km |
| `RidgeDistanceKm` | A | Haversine on `(Sample, qΓ)` where `qΓ = (Q1+Q2)/||Q1+Q2||` is hand-derived per fixture. `FFixtureSpec.ExpectedRidgeDistanceKm`. | 1×10⁻⁶ km |
| `NearestBoundaryDistanceKm` | Property: `min(ExpectedQ1, ExpectedQ2)` from above expected constants | Composition of independent constants. `FFixtureSpec.ExpectedNearestBoundaryDistanceKm`. | 1×10⁻⁶ km |
| `Alpha` | A | Hand-computed: `ExpectedRidgeDistanceKm / (ExpectedRidgeDistanceKm + ExpectedNearestBoundaryDistanceKm)`. `FFixtureSpec.ExpectedAlpha`. Note: composition of independents is independent. | 1×10⁻⁹ |
| `ZBarElevation` | A | Hand-computed: `(1-T)·Q1Elev + T·Q2Elev` with `T = ExpectedQ1DistanceKm / (ExpectedQ1DistanceKm + ExpectedQ2DistanceKm)`, written as the explicit linear combination NOT `Lerp`. `FFixtureSpec.ExpectedZBar`. | 1×10⁻⁹ km |
| `ZGammaElevation` | A | Hand-computed: `(1-ExpectedAlpha)·z_e + ExpectedAlpha·z_o` written as the explicit linear combination NOT `Lerp`, with `z_e = -1.0` and `z_o = -6.0` as named local constants in the oracle (no shared header constant). `FFixtureSpec.ExpectedZGamma`. | 1×10⁻⁹ km |
| `Elevation` | A | Hand-computed: `ExpectedAlpha·ExpectedZBar + (1-ExpectedAlpha)·ExpectedZGamma`. `FFixtureSpec.ExpectedElevation`. | 1×10⁻⁹ km |
| `OceanicAge` | Constant invariant | Always `0.0` for generated crust. `FFixtureSpec.ExpectedOceanicAge = 0.0`. | 1×10⁻¹² Ma |
| `RidgeDirection` (vector) | A — hand-derived expected unit vector | For symmetric fixtures: closed-form by ridge-axis geometry (e.g., for sample at (cos 10°, 0, sin 10°), Q1/Q2 symmetric across longitude, qΓ = (1, 0, 0): expected = (0, 1, 0)). For asymmetric fixtures: pre-computed offline. `FFixtureSpec.ExpectedRidgeDirection`. | 1×10⁻⁸ |
| `RidgeDirection` magnitude | Property invariant | Should equal 1.0 ± ε always. | 1×10⁻⁸ |
| `RidgeDirection` radial dot | Property invariant | `\|dot(RidgeDirection, Sample)\|` should equal 0.0 ± ε always. | 1×10⁻⁸ |
| Existing-fixture record-hash regression | A — burned-in expected hash literal | The six current IIIE.4 report hashes (`0fc06efc6a92253b`, `e9052ba3fa60a5dd`, `91ab557101086280`, `542d1b822c77773c`, `251e526a1f992b5e`, `e5e8edfef4290c4c`) are pasted into `FFixtureSpec.ExpectedRecordHash` for the corresponding existing fixtures. New fixtures' expected hashes are derived offline at fixture-design time. The runtime gate compares `ComputeRecordHash(Record)` against `Fixture.ExpectedRecordHash` — exact string match. | Exact string equality (no tolerance) |

### Key independence properties

1. **Different formula path for distance.** Actor uses
   `acos(clamp(dot(A, B), -1, 1))`; the oracle's burned-in expected values
   are derived from haversine `2·asin(sqrt(hav(Δlat) + cos(lat₁)·cos(lat₂)·hav(Δlon)))`.
   These are mathematically equivalent for non-degenerate inputs but trace
   different operand paths (lat/lon decomposition vs cartesian dot product),
   different transcendentals (asin·sqrt vs acos), different rounding orders.

2. **Different blend form.** Actor uses `FMath::Lerp(a, b, t)`. The oracle's
   burned-in expected values are derived from the explicit linear combination
   `(1 - t)·a + t·b` evaluated at design time. Algebraic identity is trivial
   in real arithmetic but the operand path differs in IEEE-754 (Lerp's
   internal form is `a + t·(b - a)`, which has different cancellation than
   `(1-t)·a + t·b`). The bigger independence comes from the values being
   pre-computed, not re-derived at runtime.

3. **Local-constant zΓ endpoints.** The oracle defines its own
   `OracleRidgePeakElevationKm = -1.0` and `OracleAbyssalElevationKm = -6.0`
   as local consts and uses these to derive expected ZGamma. They are NOT
   imported from the actor's `PhaseIIIE4RidgePeakElevationKm` or via a shared
   header. This means: if a future change accidentally edits the actor's
   constants, the oracle still expects -1.0 / -6.0 and will fail loudly. (If
   the change is intentional, the oracle constants must be edited in lockstep
   — that's the desired audit behavior.)

4. **Pre-computed expected values are immune to runtime arithmetic.** A
   fixture spec carrying `ExpectedAlpha = 0.31044` cannot agree with the
   actor's runtime output unless the actor really does compute that value. No
   matter what bug the actor has, the residual will not be zero unless the
   actor's runtime arithmetic agrees with the design-time hand derivation.

5. **Symmetric vs asymmetric fixtures.** Existing fixtures use
   `UnitFromLonLat(±20°, 0°)` for Q1/Q2 — perfectly symmetric, so any oracle
   that accidentally averages or substitutes Q1↔Q2 still passes. New
   asymmetric fixtures break this: expected Q1Dist ≠ Q2Dist, expected
   RidgeDirection has non-zero x and z components, etc.

### Antipodal qΓ fallback

If `Q1 + Q2 ≈ 0` (Q1 and Q2 antipodal), the actor's `qΓ = NormalizeOrFallback(Q1 + Q2, Sample)`
returns Sample. The oracle must handle this:

- New fixture `MakeAntipodalBoundaryFixture` with Q1 = (1, 0, 0), Q2 = (-1, 0, 0)
  (longitudes 180° apart on equator). Sample = (0, 1, 0).
- Hand-derived expected: `qΓ = Sample` because `||Q1+Q2|| = 0`. RidgeDistance
  = 0 (degenerate). Alpha = 0. ZGamma = z_e. Elevation = (1-0)·ZGamma + 0·ZBar
  = z_e = -1.0 km.
- Burned-in expectations: `ExpectedRidgeDistanceKm = 0.0`, `ExpectedAlpha = 0.0`,
  `ExpectedZGamma = -1.0`, `ExpectedElevation = -1.0`,
  `ExpectedRidgeDirection = (0, 0, 0)` (degenerate; magnitude check relaxed
  with `bExpectDegenerateRidgeDirection = true`).
- Gate: actor must report this exact degenerate state. If actor instead
  produces e.g. `Alpha = 0.5` from a numerical bug, the residual flags it.

### Non-positive separating velocity anomaly

Already exercised by `MakeNonSeparatingAnomalyFixture` with
`SignedSeparationVelocity = -0.002`. Oracle behavior:

- Expected `bGeneratedOceanicCrust = false`, `bNonSeparatingAnomaly = true`,
  `Elevation = 0.0`, `Alpha = 0.0`, `ZBar = 0.0`, `ZGamma = 0.0`.
- These are control-flow gates; the oracle just verifies the expected flags
  and zero scalar fields. No arithmetic to forge.

### Multi-hit rejection path

Already exercised by `MakeUnresolvedRouteRejectedFixture` and
`MakeResolvedRouteRejectedFixture`. Oracle behavior identical: expected
generation flags false, scalars zero, rejection flags true. Control-flow
gates only.

### New asymmetric fixtures (to break symmetry-only-passing oracles)

| Fixture | Sample | Q1 | Q2 | Why |
|---|---|---|---|---|
| `MakeAsymmetricElevationFixture` | UnitFromLonLat(0°, 5°) | UnitFromLonLat(-30°, 0°), Elev = -3.5 | UnitFromLonLat(15°, 0°), Elev = -1.5 | Q1Dist ≠ Q2Dist; ZBar weight ≠ 0.5; expected Alpha asymmetric. |
| `MakeOffAxisSampleFixture` | UnitFromLonLat(5°, 8°) | UnitFromLonLat(-20°, 0°), Elev = -4.0 | UnitFromLonLat(20°, 0°), Elev = -2.0 | Sample displaced from ridge perpendicular plane; expected RidgeDirection has non-trivial x-component. |
| `MakeAntipodalBoundaryFixture` | UnitFromLonLat(0°, 90°) (north pole) | UnitFromLonLat(0°, 0°) | UnitFromLonLat(180°, 0°) | Q1+Q2 = 0; tests qΓ degenerate fallback path. |

### Code-shape sketch (for Codex)

The replacement structure for `CarrierLabPhaseIIIE4Commandlet.cpp`:

```
struct FFixtureSpec
{
    // existing fields kept ...
    FString Name;
    FString Purpose;
    FVector3d Sample = FVector3d::UnitX();
    TArray<FCarrierLabPhaseIIIE2BoundaryEdgeProbe> Edges;
    ECarrierLabPhaseIIIE3SelectionClass SourceSelectionClass = ...;
    double SignedSeparationVelocity = 0.002;
    bool bExpectGenerated = true;
    // ... other expect-flag fields kept ...

    // NEW expected-value constants (derived offline / by hand):
    double ExpectedQ1DistanceKm = 0.0;
    double ExpectedQ2DistanceKm = 0.0;
    double ExpectedRidgeDistanceKm = 0.0;
    double ExpectedNearestBoundaryDistanceKm = 0.0;
    double ExpectedAlpha = 0.0;
    double ExpectedZBar = 0.0;
    double ExpectedZGamma = 0.0;
    double ExpectedElevation = 0.0;
    double ExpectedOceanicAge = 0.0;
    FVector3d ExpectedRidgeDirection = FVector3d::ZeroVector;
    bool bExpectDegenerateRidgeDirection = false;

    // Active hash-regression gate. For each existing fixture, this carries
    // the verbatim hash from the current report so any inadvertent actor
    // change fails this gate immediately. Empty string means "skip"; do
    // not skip for the existing six fixtures.
    FString ExpectedRecordHash;
};

// REPLACES ComputeIndependentOracle entirely. No runtime arithmetic.
bool VerifyAgainstFixtureExpectations(
    const FFixtureSpec& Fixture,
    const FCarrierLabPhaseIIIE4OceanicGenerationRecord& Record,
    FFixtureResult& OutResult)
{
    // Distance gates
    OutResult.Q1DistanceResidual = FMath::Abs(Record.Q1DistanceKm - Fixture.ExpectedQ1DistanceKm);
    OutResult.Q2DistanceResidual = FMath::Abs(Record.Q2DistanceKm - Fixture.ExpectedQ2DistanceKm);
    OutResult.RidgeDistanceResidual = FMath::Abs(Record.RidgeDistanceKm - Fixture.ExpectedRidgeDistanceKm);
    OutResult.NearestBoundaryDistanceResidual = FMath::Abs(Record.NearestBoundaryDistanceKm - Fixture.ExpectedNearestBoundaryDistanceKm);
    // Blend gates
    OutResult.AlphaResidual = FMath::Abs(Record.Alpha - Fixture.ExpectedAlpha);
    OutResult.ZBarResidual = FMath::Abs(Record.ZBarElevation - Fixture.ExpectedZBar);
    OutResult.ZGammaResidual = FMath::Abs(Record.ZGammaElevation - Fixture.ExpectedZGamma);
    OutResult.ElevationResidual = FMath::Abs(Record.Elevation - Fixture.ExpectedElevation);
    OutResult.OceanicAgeResidual = FMath::Abs(Record.OceanicAge - Fixture.ExpectedOceanicAge);
    // Vector gates
    if (Fixture.bExpectDegenerateRidgeDirection)
    {
        OutResult.RidgeDirectionResidual = Record.RidgeDirection.Size();   // expect zero
        OutResult.RidgeRadialDot = 0.0;
    }
    else
    {
        OutResult.RidgeDirectionResidual = (Record.RidgeDirection - Fixture.ExpectedRidgeDirection).Size();
        OutResult.RidgeRadialDot = FMath::Abs(FVector3d::DotProduct(Record.RidgeDirection, NormalizeOrFallback(Fixture.Sample, FVector3d::UnitX())));
    }
    // Active hash-regression gate. Computed from the same ComputeRecordHash
    // already used for the report; expected-hash literal lives in fixture.
    OutResult.RecordHash = ComputeRecordHash(Record);
    OutResult.bRecordHashMatch =
        Fixture.ExpectedRecordHash.IsEmpty() ||
        OutResult.RecordHash.Equals(Fixture.ExpectedRecordHash, ESearchCase::CaseSensitive);

    // Pass predicate uses tolerances from the per-gate design table AND
    // requires bRecordHashMatch.
    return ... (residuals each below their per-gate tolerance) && OutResult.bRecordHashMatch;
}
```

The fixture builders write the expected constants in literal form, with a
header comment naming the derivation:

```
FFixtureSpec MakeNoHitGenerationFixture()
{
    FFixtureSpec F;
    F.Sample = UnitFromLonLat(0.0, 10.0);
    F.SourceSelectionClass = ECarrierLabPhaseIIIE3SelectionClass::NoHitDivergentGap;
    AddDefaultDivergentEdges(F);

    // Closed-form expected values, hand-derived by haversine and explicit
    // (1-t)·a + t·b blends. Sample ⋅ Q1 = Sample ⋅ Q2 = cos(10°)·cos(20°),
    // so Q1Dist = Q2Dist; Sample ⋅ qΓ = cos(10°). qΓ = (1, 0, 0) by
    // symmetry. RidgeDirection = (0, 1, 0) by symmetry.
    F.ExpectedQ1DistanceKm                = 2469.943...;   // R · acos(cos10·cos20)
    F.ExpectedQ2DistanceKm                = 2469.943...;
    F.ExpectedRidgeDistanceKm             = 1112.005...;   // R · 10°
    F.ExpectedNearestBoundaryDistanceKm   = 2469.943...;
    F.ExpectedAlpha                       = 0.310438...;   // 1112.005 / 3581.948
    F.ExpectedZBar                        = -3.0;          // (1-0.5)·-4 + 0.5·-2
    F.ExpectedZGamma                      = -2.552190...;  // (1-α)·-1 + α·-6
    F.ExpectedElevation                   = -2.691272...;  // α·ZBar + (1-α)·ZGamma
    F.ExpectedOceanicAge                  = 0.0;
    F.ExpectedRidgeDirection              = FVector3d(0.0, 1.0, 0.0);
    F.ExpectedRecordHash                  = TEXT("0fc06efc6a92253b");
    return F;
}
```

(Decimals shown approximate. The actual literals in code must be at FULL
double precision via `'%.17g' % x` or `repr(x)` from the offline Python
script — IEEE-754 double is ~15.95 decimal digits and "14+" is not
enough to round-trip without ULP-level rounding loss.)

### Replay determinism

The replay-determinism gate (`ReplayA.RecordHash == ReplayB.RecordHash`) is
unaffected by oracle redesign and stays as-is. It already verifies a
different property (same input → same output across two runs of the actor),
which is independent of the residual gates and remains valid.

## Task 2 — Concern #2 disposition

### Re-reading the thesis

**§3.3.2.1, page 66 (`cc5c6807-077.png`):**
"L'élévation du point z(p, t+δt) dans la zone divergente est calculée par
mélange continu entre l'interpolation linéaire des élévations des plaques z̄
et un patron d'élévation représentant un profil générique de dorsale zΓ
(Figure 40): z(p, t+δt) = α z̄(p,t) + (1 − α) zΓ(p,t)"

zΓ is written `zΓ(p, t)` — explicitly a function of position and time. No
closed form is given in §3.3.2.1.

**§3.3.2.3, page 68 (`cc5c6807-079.png`):**
"À partir de ces deux points, nous pouvons procéder au calcul de la nouvelle
croûte océanique en p tel que décrit conceptuellement en 3.3.2.1. Pour ce
faire nous avons besoin d'une estimation du point le plus proche situé sur
la dorsale médiane: nous procédons simplement le milieu du segment sphérique
reliant les deux points frontière, i.e. qΓ = R · (qi + qj) / ||qi + qj||."

§3.3.2.3 only specifies qΓ. It does NOT specify the closed form of zΓ. It
says "tel que décrit conceptuellement en 3.3.2.1" — refers back to 3.3.2.1
for the conceptual formula, which is also silent on zΓ shape.

**§3.3.3 Évolution océanique, page 70 (`cc5c6807-081.png`):**
"La croûte océanique émerge et s'étend depuis les dorsales à des vitesses
géologiques; cependant à mesure que les millions d'années passent, elle
prend plus de densité et son élévation moyenne décroît. Pour reproduire
cela, nous appliquons l'affaissement suivant: z(p, t+δt) = z(p,t) (1 −
z(p,t)/z_o) ε_o δt"

§3.3.3 establishes a separate per-step temporal subsidence law applied to
all oceanic crust. Rate is small (ε_o = 4×10⁻² mm/yr per Table 3.2), so
over a single 32 Ma resample period it subsides ridge crust by only ~1 km.
This means zΓ at *generation* must span most of the −1 km → −6 km range
**spatially**; §3.3.3 cannot do the full subsidence in time alone. Figure 40
visually agrees that zΓ peaks at the ridge axis and falls toward abyssal
depth at the boundary side of the gap.

### Implication for Option A vs Option B

The audit recommended Option A (decouple zΓ from α; use a chosen reference
distance d_ref). On re-reading:

- **Thesis-text-silent.** Neither §3.3.2.1, §3.3.2.3, nor §3.3.3 prescribes
  a closed form for zΓ. The thesis lists no "ridge-subsidence reference
  distance" in Table 3.2.
- **Option A1 picks d_ref.** Candidates: r_c = 4200 km (continental
  collision), r_a = 1800 km (subduction), or invent `d_ridge_ref`. r_c and
  r_a describe physically unrelated processes (collision uplift falloff and
  subduction tracking range, respectively); borrowing them for ridge
  subsidence is a category mix. Inventing a new constant is a non-thesis
  choice that the thesis does not authorize.
- **Option B keeps current.** zΓ = `Lerp(z_e, z_o, α)`. This is a *linear*
  profile that reaches z_o at the plate boundary exactly. The slope of zΓ
  in the dΓ direction depends on dP — the deviation the audit names.
- **Figure 40 shows a curved profile.** Both A and B are *linear*
  approximations. Neither captures figure 40's curvature. Option B matches
  figure 40 in the *boundary-meeting* property (zΓ → z_o at the plate
  boundary) but not in shape; Option A with chosen `d_ref` would also be
  linear and would miss curvature too. A truly figure-40-consistent zΓ
  would be sqrt-of-distance subsidence (Parsons & Sclater 1977) or
  similar — but that's external geophysics, not a thesis citation.
- **Both are placeholders.** Neither A nor B has stronger thesis support.
  Both are linear approximations that diverge from figure 40's curvature
  but match it in different aspects (A: fixed slope; B: reaches z_o at
  boundary).

**Recommendation: Option B.** Reasons:

1. Option A introduces a NEW non-thesis constant (`d_ridge_ref`) without
   stronger thesis support than B's α reuse. The deviation moves but does
   not shrink.
2. Option B requires no actor arithmetic change. The slice can focus on
   closing concern #4 (oracle independence), which IS a real bug.
3. Figure 40 visually shows zΓ reaching z_o at the boundary side of the
   gap, which Option B captures (boundary-meeting only — figure 40 also
   shows curvature, which neither A nor B captures since both are linear).
4. §3.3.3's slow temporal affaissement does not subsume zΓ's spatial range,
   so any zΓ shape must span −1 to −6 km in space anyway.
5. A future slice that admits external geophysics literature (Parsons &
   Sclater 1977 sqrt-of-distance subsidence) can replace zΓ with a
   physically-motivated closed form. That slice should also pick the ridge
   reference distance with a published basis — neither r_c nor r_a are
   defensible for this.

### Exact deferral language (Option B)

**For the actor source**, add this comment block immediately above the zΓ
Lerp in `PopulatePhaseIIIE4OceanicRecord` (currently
`CarrierLabVisualizationActor.cpp:2427-2430`):

```
// IIIE.4 ridge-profile placeholder. Thesis §3.3.2.1 (page 66) describes zΓ
// as "un patron d'élévation représentant un profil générique de dorsale"
// with no closed form given, and writes it as zΓ(p, t) — function of
// position and time. Thesis §3.3.2.3 (page 68) refers ZGamma calculation
// back to §3.3.2.1's conceptual formula and adds no closed form. Figure 40
// illustrates zΓ as a CURVED profile peaking at z_e at the ridge axis and
// reaching z_o at the plate-boundary side of the gap. This implementation
// parameterizes zΓ by the same α used in the blend coefficient
// (α = dΓ/(dΓ + dP)) via a linear Lerp. The resulting profile reaches z_o
// exactly at the plate boundary (figure-40 boundary-meeting consistent),
// but the implementation's Lerp is LINEAR — figure 40 visually depicts a
// CURVED profile, which neither this Lerp nor any thesis-cited closed
// form captures. The further deviation: the slope of zΓ in the dΓ
// direction depends on the gap width dP, which is a deviation from a
// strict "generic ridge profile" reading of §3.3.2.1. The thesis is
// genuinely ambiguous on the closed form and Table 3.2 lists no ridge-
// subsidence reference distance; alternatives (e.g. Lerp parameterized by
// dΓ / d_ref with a chosen reference scale, or Parsons & Sclater 1977
// sqrt-of-distance subsidence which would also capture figure 40's
// curvature) require non-thesis choices and are deferred. §3.3.3 oceanic
// affaissement (per-step temporal) is too slow to span -1 km → -6 km
// within a single resample period, so zΓ must span the range spatially.
//
// TODO(IIIE-future-slice): Replace this α-based linear parameterization
// once a thesis-aligned or external-geophysics-cited closed form is
// selected. Options enumerated in
// docs/checkpoints/phase-iii-pre-iiie5-oracle-and-zgamma-design.md.
```

**For the IIIE.4 report (`docs/checkpoints/phase-iii-slice-iiie4-report.md`),
Scope section**, replace the current placeholder line:

> `zBar(p)` is independently distance-interpolated between the q1/q2
> boundary elevations. `zGamma(p)` uses the named IIIE.4 ridge-profile
> convention anchored to thesis Table 3.2 constants: ridge peak `-1 km`,
> abyssal plain `-6 km`, linearly parameterized by the same ridge-to-
> boundary alpha until a more detailed profile curve is extracted.

with:

> `zBar(p)` is independently distance-interpolated between the q1/q2
> boundary elevations. `zGamma(p)` is a NAMED PLACEHOLDER. Thesis §3.3.2.1
> (page 66) describes zΓ as "un patron d'élévation représentant un profil
> générique de dorsale" with no closed form given; §3.3.2.3 (page 68) adds
> no closed form. Figure 40 illustrates zΓ as a CURVED profile peaking at
> z_e = -1 km at the ridge axis and reaching z_o = -6 km at the
> plate-boundary side of the gap. The implementation uses
> `Lerp(z_e, z_o, α)` — a LINEAR profile — parameterized by the same α as
> the outer blend (α = dΓ/(dΓ + dP)); this reaches z_o at the boundary
> (figure-40 boundary-meeting consistent) but is linear and so does NOT
> capture figure 40's curvature, and additionally couples the slope to
> gap width dP, which is a further deviation from a strict "generic
> profile" reading. Constants `z_e = -1 km` and `z_o = -6 km` are
> verified verbatim from thesis Table 3.2 (page 51); the parameterization
> and the linear shape are the placeholder. §3.3.3 oceanic affaissement
> runs too slowly to subsume zΓ's spatial range within a single resample
> period. Replacement deferred to a future IIIE slice that admits a
> published closed form (e.g. Parsons & Sclater 1977 sqrt-of-distance
> subsidence, which would also capture figure 40's curvature). See
> `docs/checkpoints/phase-iii-pre-iiie5-zgamma-audit.md` and
> `docs/checkpoints/phase-iii-pre-iiie5-oracle-and-zgamma-design.md`.

## Implementation packet for Codex

### Slice scope (single commit)

Title: "Close IIIE.4 oracle independence and name zΓ placeholder."

### Files to touch

| File | Change | Approx scope |
|---|---|---|
| `Source/CarrierLab/Private/CarrierLabPhaseIIIE4Commandlet.cpp` | Extend `FFixtureSpec` with expected-value fields. Replace `ComputeIndependentOracle` with `VerifyAgainstFixtureExpectations`. Update each fixture builder (`MakeNoHitGenerationFixture`, `MakeFilterExhaustedGenerationFixture`, `MakeNonSeparatingAnomalyFixture`, `MakeResolvedRouteRejectedFixture`, `MakeUnresolvedRouteRejectedFixture`, `MakeNoBoundaryPairFixture`) with hand-derived expected constants. Add three new fixtures (`MakeAsymmetricElevationFixture`, `MakeOffAxisSampleFixture`, `MakeAntipodalBoundaryFixture`). Update `RunFixture` to use the new verification function. Update `BuildJsonLine` and `BuildReport` if new gate columns are added. | Roughly +200 / -50 lines. The bulk is fixture-builder bodies and expected-value literals. `ComputeIndependentOracle` removed. |
| `Source/CarrierLab/Private/CarrierLabVisualizationActor.cpp` | Insert the named-placeholder comment block immediately above the zΓ Lerp at line 2427–2430 (do not change arithmetic). | +20 lines comment, 0 lines arithmetic. |
| `docs/checkpoints/phase-iii-slice-iiie4-report.md` | Replace the placeholder line in Scope as specified above. | -1 / +12 lines. |

### Expected-value derivation (offline computation step)

Codex should generate expected-value constants by running a one-off Python
script (kept local, not committed) that:

1. Loads each fixture's `Sample, Q1, Q2, Q1Elevation, Q2Elevation` from its
   builder.
2. Computes distances using `haversine`:
   `d = R · 2·asin(sqrt(sin²(Δlat/2) + cos(lat₁)·cos(lat₂)·sin²(Δlon/2)))`.
3. Computes qΓ as `(Q1+Q2) / ||Q1+Q2||` (with antipodal fallback to Sample).
4. Computes ZBar, Alpha, ZGamma, Elevation as explicit `(1-t)·a + t·b`
   linear combinations with z_e = -1.0, z_o = -6.0 hard-coded as Python
   literals.
5. Computes RidgeDirection by `((Sample - qΓ) × Sample)` retangented and
   normalized (this part has the same form as the actor; the independence
   for direction comes from offline pre-computation, not formula change).
6. Outputs literal C++ initializer lines to paste into the fixture builders,
   at FULL DOUBLE PRECISION. IEEE-754 double is ~15.95 decimal digits, so
   "14+" is not enough — round-tripping a Python `float` through `repr()`
   and into a C++ `double` literal without loss requires `repr(x)` or
   `format(x, '.17g')`. Use `'%.17g' % x` (or equivalent) when emitting
   each literal. Otherwise the burned-in constants accumulate ~1 ULP of
   rounding before they ever enter the gate, and that error competes with
   the 1×10⁻⁹ tolerance budget.

The Python script is a derivation tool, not a test artifact. After the
literals are pasted into the C++ source, the script can be discarded
(or kept under `docs/scripts/` as documentation if Codex prefers).

### Gates table the regenerated IIIE.4 report should show

After the slice lands, re-running the IIIE.4 commandlet should produce:

| Gate | Result | What changed |
|---|---:|---|
| no-hit divergent oceanic generation | pass | residuals now compared against pre-derived constants, not re-computed Lerps |
| filter-exhausted divergent oceanic generation | pass | same |
| non-separating q1/q2 anomaly | pass | control-flow gate, expected zeros |
| resolved single-hit route rejected | pass | same |
| unresolved multi-hit route rejected | pass | same |
| no two-plate boundary pair anomaly | pass | same |
| asymmetric elevation generation | pass (NEW) | expected Q1Dist ≠ Q2Dist; ZBar weight ≠ 0.5 |
| off-axis sample generation | pass (NEW) | expected RidgeDirection has non-zero x and z components |
| antipodal boundary qΓ degenerate | pass (NEW) | expected qΓ-fallback path; degenerate RidgeDirection allowed |
| Existing-fixture record-hash regression | pass (NEW) | active gate. The six current report hashes (`0fc06efc6a92253b`, `e9052ba3fa60a5dd`, `91ab557101086280`, `542d1b822c77773c`, `251e526a1f992b5e`, `e5e8edfef4290c4c`) are burned into `FFixtureSpec.ExpectedRecordHash` and verified at runtime. Any inadvertent actor arithmetic change fails this gate immediately. |
| Same-seed oceanic-generation replay | pass | unchanged |

The metrics JSONL adds the three new fixtures' rows and the new per-gate
residuals. Hashes for the existing six fixtures are now an ACTIVE GATE
(burned into `FFixtureSpec.ExpectedRecordHash` and verified at runtime,
not just passively observed): the actor's runtime arithmetic is untouched,
so the existing hashes must equal their literal expectation, and any
inadvertent change fails the new regression gate immediately.

### Acceptance criteria

1. `ComputeIndependentOracle` is removed entirely.
2. `VerifyAgainstFixtureExpectations` (or equivalently named) reads only
   `Fixture.ExpectedXxx` fields; performs no Lerps, no haversine, no
   distance computation.
3. Every existing fixture builder has expected constants for: Q1Dist, Q2Dist,
   RidgeDist, NearestBoundaryDist, Alpha, ZBar, ZGamma, Elevation,
   OceanicAge, RidgeDirection (with `bExpectDegenerateRidgeDirection`
   where appropriate), and `ExpectedRecordHash` (verbatim from the
   current IIIE.4 report for the existing six fixtures).
4. Three new asymmetric fixtures land with hand-derived expected constants
   AND hand-derived expected record hashes (computed offline from the
   `ComputeRecordHash` formula applied to the expected record fields, or
   captured from a first-run instrumentation pass and then verified by a
   second clean run before the literal is committed).
5. The actor's `PopulatePhaseIIIE4OceanicRecord` arithmetic is unchanged.
6. The actor source has the named-placeholder comment block above the zΓ
   Lerp.
7. The IIIE.4 report's Scope line is updated with the deferral language.
8. Regenerated `phase-iii-slice-iiie4-report.md` shows all old + new gates
   pass (including the active record-hash regression gate); replay
   determinism passes.
9. Regenerated metrics file (`phase-iii-slice-iiie4-metrics.jsonl`) has
   nine fixtures plus replay row, with `record_hash` matching
   `expected_record_hash` for every fixture.
10. The six existing fixtures' record hashes literally equal
    `0fc06efc6a92253b`, `e9052ba3fa60a5dd`, `91ab557101086280`,
    `542d1b822c77773c`, `251e526a1f992b5e`, `e5e8edfef4290c4c` (in fixture
    order: NoHit, FilterExhausted, NonSeparating, ResolvedRejected,
    UnresolvedRejected, NoBoundaryPair). Mapping verified against the
    current `phase-iii-slice-iiie4-report.md`.

### What this slice does NOT do

- Does not change the actor's zΓ arithmetic (concern #2 stays as a named
  placeholder, not corrected).
- Does not introduce a new ridge-subsidence reference distance.
- Does not add q vs qΓ residual fixtures (audit's open question #1 reframed:
  thesis §3.3.2.3 explicitly defines qΓ as the implementation estimation of
  q, so IIIE.4 is faithful here, not deviating).
- Does not revisit IIIE.2's qΓ formula (audit's open question #3 closed:
  thesis page 68 formula `qΓ = R · (qi+qj)/||qi+qj||` matches IIIE.2
  implementation verbatim).
- Does not block IIIE.5 beyond its own gate. After this slice lands, IIIE.5
  may proceed.

## Open questions

1. **Whether `bExpectDegenerateRidgeDirection` belongs at the FFixtureSpec
   level or inside the verifier.** Design says FFixtureSpec; alternative is
   to derive it from `ExpectedRidgeDirection.Size() < ε`. Codex's call.
   Recommend keeping the explicit boolean to make the intent obvious in the
   fixture body.

2. **Where the offline derivation script lives.** Recommend `docs/scripts/`
   as a non-build-system documentation directory. If Codex prefers, keep it
   only locally during the slice and reference the technique in a short
   header comment near the fixture builders.

3. **Tolerance choices.** The table gives concrete values (1e-6 km for
   distances, 1e-9 for dimensionless, 1e-9 km for elevations, 1e-8 for unit
   vectors). These are conservative bounds chosen to allow some IEEE-754
   rounding but tight enough to catch any real arithmetic divergence. If
   any new fixture's residual exceeds tolerance after the literals are
   pasted, the most likely cause is insufficient precision in the literal
   (use `'%.17g'` or `repr(x)` for the offline emit — anything less drops
   ULP-level digits and bites into the 1e-9 tolerance); the second-most-
   likely is a real bug in the actor.

4. **Whether to also burn in expected `Q1UnitPosition`, `Q2UnitPosition`,
   `QGammaUnitPosition`** — the unit vector fields the actor copies from
   `BoundaryAudit`. Currently the fixture's `ExpectedQ1`, `ExpectedQ2` cover
   the input side, but qΓ is computed inside IIIE.2. A `qΓ identity`
   property gate (qΓ should equal `Normalize(Q1+Q2)` to within ε) is cheap
   and worth adding. Recommend adding it.

## Recommendation

Recommend a single-commit slice titled "Close IIIE.4 oracle independence and
name zΓ placeholder," scoped to:

1. Replace `ComputeIndependentOracle` with `VerifyAgainstFixtureExpectations`
   that compares actor output against pre-derived constants (Technique A,
   closed-form-per-fixture).
2. Extend `FFixtureSpec` with the expected-value fields enumerated in the
   per-gate table, including `ExpectedRecordHash` for the active hash-
   regression gate.
3. Add hand-derived expected constants at full double precision
   (`'%.17g'` / `repr(x)`, ≥17 significant digits) to all six existing
   fixtures, plus their verbatim `ExpectedRecordHash` from the current
   IIIE.4 report.
4. Add three new asymmetric fixtures (`MakeAsymmetricElevationFixture`,
   `MakeOffAxisSampleFixture`, `MakeAntipodalBoundaryFixture`) with
   hand-derived expected scalar/vector constants AND hand-derived
   `ExpectedRecordHash` values.
5. Insert the named-placeholder comment block above the actor's zΓ Lerp,
   explicitly naming the linear-vs-curved figure-40 mismatch alongside
   the α-double-use deviation; leave actor arithmetic untouched.
6. Update the IIIE.4 report's Scope line with the deferral language
   (linear approximation of figure 40's curve, plus α-coupling
   deviation).

Concern #2 stays as a NAMED placeholder (Option B). Concern #4 closes
because the runtime gate now verifies actor output against algebraically-
derived design-time constants instead of re-running the same arithmetic;
the active hash-regression gate further protects against any inadvertent
actor change. **IIIE.5 unblocks immediately on commit** — the audit's
CRITICAL gap is closed, the deviation is now formally deferred with
explicit thesis citation, and the hash gate guards the existing arithmetic
against silent drift. No re-run of IIIE.4 verification beyond what this
slice's own gates demonstrate is required.
