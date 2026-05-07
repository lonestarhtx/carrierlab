# Pre-IIIH.0 zΓ Ridge-Profile Research

Research checkpoint preceding any replacement of the Pre-IIIE.5.1 zΓ
distance-profile placeholder
(`bUsedZGammaDistanceProfilePlaceholder = true`,
`bPaperFaithfulZGammaProfile = false`). Scope: identify whether the thesis
or CGF paper supplies a closed form previously missed, characterize the
external geophysics literature that would otherwise be the citation source,
and recommend a replacement closed form with an explicit citation chain
and confidence level.

This document is research only. No code change, no slice specification.

## Status

**Recommendation ready, with one user decision required.**

Both the Cortial thesis (§3.3.2.1, §3.3.2.3, §3.3.3) and the CGF 2019 paper
(§4.3, §4.5) are confirmed silent on a closed form for zΓ. Fresh re-reads
of pages 65–70 of the thesis and pages 4–7, 12 of the CGF paper turn up no
equation that prior audits missed. Table 3.2 (thesis p. 51) and the CGF
paper's Appendix A constants table (p. 12) list `r_a = 1800 km`
(subduction) and `r_c = 4200 km` (collision) but no ridge-distance
reference; the only ridge-related constants are `z_e = -1 km` and
`z_o = -6 km`.

The recommendation is therefore **geophysics-derived** (Parsons & Sclater
1977 sqrt-of-age subsidence, mapped to distance via half-spreading rate),
not thesis-faithful or paper-faithful. The user decision required: accept
external geophysics literature as authority for this slice, or keep the
named placeholder indefinitely.

## Source 1 — Thesis §3.3.2 / §3.3.3 fresh re-read

Image-to-page offset: page = image − 11. Pages 65–68 cover §3.3.2.1
(Génération du plancher océanique) and §3.3.2.3 (Implementation); pages
69–70 cover §3.3.3 (Évolution océanique).

### §3.3.2.1, page 66 (cc5c6807-077.png)

Conceptual blend formula, verbatim:

> "Soit un point d'intérêt p situé dans la zone de divergence. Soit dΓ(p)
> et dP(p) les distances depuis p respectivement à la dorsale et à la
> plaque divergente la plus proche (Figure 39). Nous définissons le
> facteur interpolant α = dΓ(p)/(dΓ(p) + dP(p)). L'élévation du point
> z(p, t+δt) dans la zone divergente est calculée par mélange continu
> entre l'interpolation linéaire des élévations des plaques z̄ et un
> patron d'élévation représentant un profil générique de dorsale zΓ
> (Figure 40):
> z(p, t+δt) = α z̄(p,t) + (1 − α) zΓ(p,t)"

zΓ is written `zΓ(p, t)` — function of position and time. The phrase
"un patron d'élévation représentant un profil générique de dorsale" is
the only characterization of its shape. **No closed form is given.**
Figure 40 illustrates zΓ as a smooth curved profile from z_e at the
ridge axis to z_o at the abyssal floor.

### §3.3.2.3, page 68 (cc5c6807-079.png)

The implementation section refers back to §3.3.2.1 for the conceptual
formula and only adds the qΓ definition:

> "Pour cela nous avons besoin d'une estimation du point le plus proche
> situé sur la dorsale médiane: nous procédons simplement le milieu du
> segment sphérique reliant les deux points frontière, i.e.
> qΓ = R · (qi + qj) / ||qi + qj||."

(Plus the ridge-direction formula `r(p) = (p − qΓ) × p`, on page 66.)
**No closed form for zΓ; no ridge-distance reference scale.**

### §3.3.3, pages 69–70 (cc5c6807-080.png, cc5c6807-081.png)

§3.3.3 contains two distinct formulas applied per timestep δt to
already-existing samples (NOT at oceanic generation):

- Continental erosion (page 69, for samples with x_C > 0.5):
  z(p, t+δt) = z(p, t) − (z(p,t)/z_c)·ε_c·δt
- Oceanic affaissement (page 70):
  z(p, t+δt) = z(p, t) − (1 − z(p,t)/z_o)·ε_o·δt

Constants from Table 3.2: ε_c = 3×10⁻³ mm/yr, ε_o = 4×10⁻² mm/yr,
z_c = 10 km, z_o = -6 km.

§3.3.3 is **temporal evolution after generation**, not the spatial
profile at generation. The spatial profile zΓ is consumed at oceanic
generation only (per §3.3.2.1's blend formula).

### Verdict for Source 1

**No closed form was missed.** The thesis is genuinely silent on the
shape of zΓ. The prior audit's conclusion stands. Three thesis pages
(§3.3.2.1, §3.3.2.3, §3.3.3) confirm:

1. zΓ is described conceptually as "patron générique de dorsale," shown
   curved in Figure 40, but never given an equation.
2. §3.3.2.3 adds qΓ = R·(qi+qj)/||qi+qj|| but no zΓ closed form.
3. §3.3.3 is a separate, slow temporal subsidence rule, NOT a candidate
   for the spatial zΓ shape.
4. Table 3.2 supplies z_e = -1 km and z_o = -6 km but no ridge-distance
   reference.

## Source 2 — CGF 2019 paper

Present at `docs/ProceduralTectonicPlanets/ProceduralTectonicPlanets.pdf`
(also as page images `aa42e52c-01.png` … `aa42e52c-12.png`).

### §4.3 Oceanic crust generation, page 7 (aa42e52c-07.png)

Verbatim, formula and surrounding text:

> "Let dΓ(p) and dP(p) be the distances from p to the ridge and to the
> closest plate respectively (Figure 8). We define the interpolating
> factor as α = dΓ(p)/(dΓ(p) + dP(p)). The elevation of the new points
> z(p,t) is computed by blending the linearly interpolated elevation
> between plates z̄ with a template ridge function profile zΓ
> (Figure 9):
> z(p, t+δt) = α z̄(p,t) + (1 − α) zΓ(p,t)"

zΓ is called a "template ridge function profile" with reference to
Figure 9 — a curved profile, just like Figure 40 of the thesis.
**No closed form is given.**

### §4.5 Continental erosion and oceanic dampening, page 7

Verbatim:

> "Oceanic dampening. As the oceanic crust spreads from the mid-ocean
> ridge and ages, its density increases and its elevation decreases.
> We apply the following dampening:
> z(p, t+δt) = z(p,t) − (1 − z(p,t)/z_o)·ε_o·δt"

Same formula as thesis §3.3.3. The minus sign that the prior audit
quoted is unambiguous in the CGF paper text.

### Appendix A — Constants, page 12 (aa42e52c-12.png)

Same set of constants as thesis Table 3.2: δt = 2 Ma, R = 6370 km,
z_e = -1 km, z_o = -6 km, z_i = -10 km, z_c = 10 km, r_a = 1800 km
(subduction reference distance), r_c = 4200 km (collision reference
distance), Δ_c = 1.3×10⁻³ km⁻¹, v_s = 8 mm/yr, v_max = 100 mm/yr,
ε_o = 4×10⁻² mm/yr, ε_e = 3×10⁻³ mm/yr, ε_s = 3×10⁻³ mm/yr,
ε_v = 6×10⁻³ mm/yr.

**No ridge-distance reference constant** in the CGF table either.

### Verdict for Source 2

The CGF 2019 paper adds **no closed form** for zΓ beyond what the thesis
already states. Figure 9 (CGF) and Figure 40 (thesis) depict the same
curved template profile illustratively, with no equation. The CGF paper
confirms — by repetition and by its constants table — that the authors
deliberately left zΓ as an unspecified template.

## Source 3 — External geophysics literature

The standard reference for ocean-floor depth as a function of crustal
age is Parsons & Sclater 1977. Subsequent refinements (Stein & Stein
1992 GDH1) tighten the asymptote but keep the same sqrt-of-age scaling
for young crust. Half-spreading rate × age gives distance from ridge
axis.

### Parsons & Sclater 1977

**Citation:** B. Parsons and J. G. Sclater, "An Analysis of the
Variation of Ocean Floor Bathymetry and Heat Flow with Age," *Journal
of Geophysical Research*, vol. 82, no. 5, pp. 803–827, 1977.

**Closed form, young ocean floor (sqrt-of-age regime):**

> d(t) = 2500 + 350·√t   meters,   for 0 < t < 70 Ma  (eastern Pacific)

Slope coefficient is ≈350 m/√Ma in the eastern Pacific and ≈390 m/√Ma
in the Atlantic and Indian oceans.

Source for verbatim formula: Imperial-College computational primer
"Seafloor ages" page
(<https://primer-computational-mathematics.github.io/book/d_geosciences/Miscellaneous/Seafloor_ages.html>),
quoting Parsons & Sclater 1977 directly: "d(t) = 2500 + 350·t^(1/2),
where 0 < t < 70 Ma."

**Closed form, older ocean floor (cooling-plate regime):**

> d(t) = 6400 − 3200·exp(−t/62.8)   meters

Source: Wikipedia article "Seafloor depth versus age"
(<https://en.wikipedia.org/wiki/Seafloor_depth_versus_age>) quoting the
1977 cooling plate model.

**Transition age:** the sqrt-of-age fit is good for t < 70 Ma; the
exponential plate-cooling fit is better for t > 80 Ma. Between
20 and 70 Ma both forms fit reasonably.

### Stein & Stein 1992 (GDH1)

**Citation:** C. A. Stein and S. Stein, "A model for the global
variation in oceanic depth and heat flow with lithospheric age,"
*Nature*, vol. 359, no. 6391, pp. 123–129, 1992.

GDH1 keeps the sqrt-of-age form for t < ~20 Ma and uses an exponential
asymptote for t > 20 Ma:

> d(t) = 2600 + 365·√t   m,   for t ≤ 20 Ma
> d(t) = 5651 − 2473·exp(−t/36)   m,   for t > 20 Ma

(The 2600/365/5651/36 constants are widely-cited summaries of the
Stein & Stein 1992 fit; the original paper presents them in tabular
fit form.)

Source for the constants summary: corroborated by multiple secondary
references in the search above; the canonical primary citation is the
Nature 1992 paper.

### Half-spreading rates on Earth

Typical half-spreading rates (one side of the ridge) range from
~10 mm/yr (slowest, Gakkel Ridge) to ~80 mm/yr (fastest historical,
East Pacific Rise). Common ranges:

- Slow (Mid-Atlantic Ridge): ~10–25 mm/yr
- Intermediate: ~25–45 mm/yr
- Fast (East Pacific Rise): ~50–80 mm/yr

Source: Wikipedia "Mid-ocean ridge" and "Seafloor spreading."

### Distance from ridge as a function of age

For oceanic crust, age and distance from the ridge axis are related by
the local half-spreading rate v_h:

> dΓ ≈ t · v_h

So Parsons & Sclater's age limit of 70 Ma maps to a distance limit of:

- 70 Ma · 25 mm/yr = 1750 km (slow ridge)
- 70 Ma · 35 mm/yr = 2450 km (intermediate ridge, global mean)
- 70 Ma · 50 mm/yr = 3500 km (fast ridge)

For Stein-Stein the asymptote (>3·τ ≈ 100 Ma at e-folding 36 Ma) maps
to:

- 100 Ma · 25 mm/yr = 2500 km
- 100 Ma · 35 mm/yr = 3500 km
- 100 Ma · 50 mm/yr = 5000 km

### Verdict for Source 3

The geophysics literature gives a published, citable closed form for
ridge subsidence: square-root-of-age. Combined with a half-spreading
rate, this becomes square-root-of-distance. The reference distance at
which subsidence reaches abyssal floor is between 1750 and 3500 km,
depending on whether one anchors to Parsons & Sclater's 70 Ma age limit
and which half-spreading rate one assumes for "typical Earth."

## Recommended closed form

Subject to the user accepting external geophysics literature as
authority for this slice (see Open questions), the recommended
replacement is the **square-root-of-distance** form:

> zΓ(dΓ) = z_e + (z_o − z_e) · √(clamp(dΓ / d_ref, 0, 1))

with:

- z_e = −1 km (thesis Table 3.2, paper Appendix A)
- z_o = −6 km (thesis Table 3.2, paper Appendix A)
- d_ref ∈ [1750 km, 3500 km] (Parsons & Sclater 1977 age limit
  combined with a half-spreading rate)

### Recommended d_ref

**Recommended d_ref = 3000 km.** Rationale:

- 70 Ma · 35 mm/yr ≈ 2450 km lies in the fast-mid-Atlantic / slow
  Pacific range; rounding to 3000 km gives a slightly conservative
  asymptote that approaches z_o = −6 km only at large dΓ.
- 100 Ma at 30 mm/yr ≈ 3000 km matches the Stein-Stein asymptote
  scale at typical-Earth half-rate.
- 3000 km is also a defensible round number for "the dΓ at which
  oceanic crust is essentially flat at abyssal depth"; it sits between
  the conservative slow-ridge value (1750 km) and the fast-ridge value
  (3500 km).
- 3000 km is NOT one of the existing thesis Table 3.2 reference
  distances (r_a = 1800 km, r_c = 4200 km), so it does not
  category-mix with subduction or collision physics.

The user could equally pick d_ref = 1750 km (Parsons & Sclater limit at
slow half-rate), 2500 km (Stein-Stein 3·τ at slow), or 3500 km (P&S
limit at fast half-rate). All are within the geophysics-literature
range. The slice plan should record whichever value the user picks,
along with the citation chain (P&S age limit + chosen v_h).

### Citation chain (confidence level: GEOPHYSICS-DERIVED)

```
zΓ(dΓ) = z_e + (z_o - z_e) · √(clamp(dΓ / d_ref, 0, 1))
        ───────┬──────       ───────┬─────────────────
        thesis Table 3.2     external geophysics:
        (paper-faithful for   sqrt-of-age subsidence
         endpoints)           [Parsons & Sclater 1977]
                              age-to-distance via
                              half-spreading rate
                              (typical Earth ~25-50 mm/yr)
                              age limit ~70 Ma
                              ───────┬─────────────────
                              d_ref ∈ [1750, 3500] km
                              recommended: 3000 km
                              ───────┴─────────────────
```

This recommendation is **geophysics-derived, not thesis-faithful** and
not CGF-paper-faithful. The endpoints `z_e` and `z_o` are
paper-faithful (Table 3.2 verbatim); the curve shape and reference
distance are sourced from external geophysics. A future paper-faithful
slice would still mark `bPaperFaithfulZGammaProfile = false` because
neither the thesis nor the CGF paper specifies a curve shape; setting
that flag to true would require an authoritative source the thesis
itself defers to.

## Numerical sanity check

zΓ at typical gap widths, comparing the current placeholder
(`Lerp(z_e, z_o, dΓ / EarthRadiusKm)` with EarthRadius = 6371 km) to
the sqrt-of-distance recommendation at d_ref = 3000 km, plus the
extreme reference distances 1750 km and 3500 km:

| dΓ (km) | placeholder (km) | d_ref=1750 (km) | d_ref=3000 (km) | d_ref=3500 (km) |
|---:|---:|---:|---:|---:|
| 100 | -1.08 | -2.20 | -1.91 | -1.85 |
| 250 | -1.20 | -2.89 | -2.44 | -2.34 |
| 500 | -1.39 | -3.67 | -3.04 | -2.89 |
| 1000 | -1.78 | -4.78 | -3.89 | -3.67 |
| 1500 | -2.18 | -5.63 | -4.54 | -4.27 |
| 2000 | -2.57 | -6.00 | -5.08 | -4.78 |
| 2500 | -2.96 | -6.00 | -5.56 | -5.23 |
| 3000 | -3.35 | -6.00 | -6.00 | -5.63 |
| 3500 | -3.75 | -6.00 | -6.00 | -6.00 |

Observations:

- The current placeholder reaches z_o = −6 km only at dΓ ≈ 6371 km,
  which is the Earth radius — i.e., the antipodal point. Real Earth
  ocean basins reach abyssal depth at dΓ ≈ 1000-3500 km, not at the
  antipode. The placeholder is dramatically too shallow.
- All three d_ref choices in the recommended sqrt-of-distance form
  produce realistic abyssal depths at typical ocean-basin gap widths.
- d_ref = 3000 km gives -3.04 km at 500 km, -3.89 km at 1000 km,
  -5.08 km at 2000 km, and reaches -6 km at exactly 3000 km. This
  matches Earth ocean basins to first order: e.g., the Mid-Atlantic
  Ridge axis to abyssal plain on each flank is roughly 1500-2000 km
  in width, with depth reaching ~5 km at 1000-1500 km from the axis.

## Interaction with §3.3.3 affaissement

§3.3.3 oceanic affaissement is a **temporal** evolution applied per
timestep to already-existing samples:

> z(p, t+δt) = z(p,t) − (1 − z(p,t)/z_o)·ε_o·δt

with ε_o = 4×10⁻² mm/yr = 0.04 km/Ma. The continuous-limit ODE
`dz/dt = -(1 - z/z_o)·ε_o` has solution
`u(t) = u(0)·exp(t·ε_o/z_o)` where `u = z - z_o`. Time constant
τ = -z_o/ε_o = 6/0.04 = **150 Ma**.

Subsidence from z(0) = -1 km (ridge crest) under §3.3.3 alone:

| t (Ma) | z(t) (km) | subsided (km) |
|---:|---:|---:|
| 16 | -1.51 | 0.51 |
| 32 | -1.96 | 0.96 |
| 64 | -2.74 | 1.74 |
| 128 | -3.87 | 2.87 |
| 256 | -5.09 | 4.09 |

At a typical Earth half-spreading rate (25-50 mm/yr), 32 Ma of
subsidence corresponds to dΓ = 800-1600 km of crustal age. §3.3.3
alone would give z ≈ -2 km at that age, while Parsons & Sclater 1977
predicts z ≈ -3.5 to -4 km. **§3.3.3 is intrinsically too slow to
produce paper-faithful or geophysics-faithful ridge profiles by itself
within a single resample period.**

This means:

1. **Spatial zΓ at generation must span most of the −1 to −6 km range
   spatially**, because §3.3.3 cannot do it in time over the 32 Ma
   resample window.
2. **Spatial zΓ and §3.3.3 do NOT double-count.** §3.3.3 has the
   `(1 − z/z_o)` factor that asymptotes to zero at z = z_o, so any
   crust that starts at zΓ_spatial(dΓ) somewhere between z_e and z_o
   is gradually nudged toward z_o by §3.3.3, not pushed past it.
3. **Spatial zΓ encodes the spatial integration of subsidence
   history**; §3.3.3 provides slow continued subsidence on top.
   Conceptually: zΓ ≈ Parsons-Sclater profile at the moment of
   generation; §3.3.3 ≈ slow drift afterward. They are complementary,
   not redundant.

This is consistent with the IIIE.4 audit's prior framing: "spatial
zΓ initializes the ridge profile at generation; §3.3.3 evolves it
temporally with time."

## Cross-check against existing extraction docs

Both `docs/paper-resampling-extraction.md` and
`docs/phase-iii-paper-process-design.md` describe zΓ at the conceptual
level only — they reproduce the thesis blend formula and call zΓ "a
generic ridge profile (peak at the ridge, dropping to abyssal depth at
distance)" without any closed form. Neither doc claims a paper-cited
shape. This matches the thesis and CGF paper findings: the closed form
genuinely does not exist in either primary source, and the extraction
docs do not pretend otherwise.

`docs/phase-iii-paper-process-design.md` IIIG section gives §3.3.3
oceanic dampening with a typo (`z_t` instead of `z_o`); the formula
sign and structure are otherwise correct relative to the thesis page
70 image and the CGF paper page 7 quote. (Out-of-scope for this
research checkpoint; flag for IIIG slice planning.)

## Open questions / user decisions needed

1. **Authority decision.** The thesis and CGF paper are silent on
   zΓ's closed form. The natural replacement is Parsons & Sclater
   1977's sqrt-of-age law mapped to distance via half-spreading rate.
   Does the user accept external geophysics literature as authority
   for this slice, knowing that any resulting record will still carry
   `bPaperFaithfulZGammaProfile = false` (because neither primary
   source authorizes the shape)? If yes, the recommendation above
   stands. If no, the placeholder remains indefinitely with no path
   to a paper-faithful elevation claim.

2. **d_ref choice.** Within the geophysics-acceptable range, the user
   should pick d_ref. Three defensible choices:
   - **1750 km** — Parsons & Sclater 70 Ma at slow half-rate
     (25 mm/yr); models Mid-Atlantic-Ridge-style basins; conservative
     (reaches z_o earliest).
   - **3000 km** (recommended) — global average ~70 Ma at ~35-40
     mm/yr or ~100 Ma at slower; sits between slow and fast Earth
     ridges.
   - **3500 km** — Parsons & Sclater 70 Ma at fast half-rate
     (50 mm/yr); models East-Pacific-Rise-style basins; reaches z_o
     latest.

3. **Curve form choice.** Square-root-of-distance is the geophysics
   default but is not the only option. Alternatives:
   - **Quadratic / quartic** — purely empirical curve fit to Figure 9
     of CGF or Figure 40 of thesis. Has no geophysics basis but
     visually mimics the figure.
   - **Stein-Stein 1992 exponential asymptote** — tighter fit for
     older crust but adds a second constant (e-folding distance) and
     requires distance-only mapping of the GDH1 piecewise form.
   - **Half-space cooling solution** (full 1D heat equation) — the
     theoretically correct form behind P&S 1977; significantly more
     complex and adds no perceptual benefit over sqrt at the
     resolutions CarrierLab operates at.
   The recommended sqrt-of-distance is the simplest published form
   that is monotonic, smooth, has the right asymptote, and has a
   defensible reference. Recommend not exploring exotic curves
   without strong cause.

4. **Replace or add a flag.** Two approaches to record-stamping:
   - **Replace** the placeholder. Set
     `bUsedZGammaDistanceProfilePlaceholder = false` and rename the
     parameter (`ZGammaProfileT`, `ZGammaProfileReferenceDistanceKm`)
     to reflect Parsons-Sclater origin (e.g.,
     `ZGammaProfileSqrtT`, `ZGammaSubsidenceReferenceDistanceKm`).
     `bPaperFaithfulZGammaProfile` stays `false`.
   - **Add** a new boolean
     `bUsedZGammaSqrtSubsidenceProfile = true` alongside the
     placeholder boolean (which goes to `false`). Future slices can
     then add a third option without churning the schema.
   The slice should pick based on whether the user expects further
   alternative profiles. Recommend "replace" for simplicity unless
   the user signals otherwise.

5. **What changes in `Source/CarrierLab/Private/CarrierLabVisualizationActor.cpp`?**
   The current implementation uses `ZGammaProfileT = dGamma / EarthRadiusKm`
   followed by `Lerp(z_e, z_o, ZGammaProfileT)`. The replacement would
   be `ZGammaProfileT = sqrt(clamp(dGamma / d_ref, 0, 1))` followed
   by the same lerp, where `d_ref` is a new module-level constant
   (e.g., `PhaseIIIE4ZGammaSubsidenceReferenceDistanceKm = 3000.0`).
   The named-placeholder comment block above the Lerp would need to
   be revised to cite Parsons & Sclater 1977 and the chosen d_ref.
   The IIIE.4 commandlet's expected-value constants (`ExpectedZGamma`,
   `ExpectedZGammaProfileT`, `ExpectedZGammaProfileReferenceDistanceKm`)
   would need to be re-derived offline at full double precision and
   re-burned into the fixture specs; existing fixture record hashes
   would all change. (Out of scope for this research checkpoint, but
   noted for slice planning.)

## Recommended next step

The research is sufficient. The user should make the **authority
decision** in Open question #1; if accepted, spawn a design agent to
specify the replacement slice (curve form, d_ref, flag scheme,
fixture constant re-derivation, comment-block revision, IIIE.4
hash rebake). If declined, the placeholder stays and any
elevation-quality claims in IIIH validation must be qualified
accordingly.

---

## Summary

The Cortial thesis (§3.3.2.1, §3.3.2.3, §3.3.3, pages 65–70 of the HAL
PDF) and the CGF 2019 paper (§4.3, §4.5, Appendix A, pages 4–7 and 12)
are both confirmed silent on a closed form for the divergent ridge
profile zΓ; both describe it only as a "patron générique de dorsale" /
"template ridge function profile" referenced to a curved figure
(thesis Figure 40, CGF Figure 9) with no equation, and neither lists a
ridge-distance reference scale among the constants in thesis Table 3.2
or CGF Appendix A. The natural replacement for the current
`Lerp(z_e, z_o, dΓ / EarthRadiusKm)` placeholder is the
square-root-of-distance form
zΓ(dΓ) = z_e + (z_o − z_e)·√(clamp(dΓ/d_ref, 0, 1)) with z_e = −1 km
and z_o = −6 km from thesis Table 3.2 and d_ref derived from Parsons &
Sclater 1977's 70 Ma age limit times a typical Earth half-spreading
rate (25–50 mm/yr), giving d_ref ∈ [1750, 3500] km — recommended
3000 km. The recommendation is **geophysics-derived, not thesis- or
paper-faithful**, and a future record using it would still carry
`bPaperFaithfulZGammaProfile = false`. Numerical comparison shows the
current placeholder reaches abyssal floor only near the antipode
(EarthRadius = 6371 km), giving zΓ = −1.4 km at 500 km, −1.8 km at
1000 km, and −2.6 km at 2000 km, dramatically shallower than realistic
Earth ocean basins; the recommended form gives −3.0 km at 500 km,
−3.9 km at 1000 km, and −5.1 km at 2000 km. The spatial zΓ and §3.3.3
temporal affaissement do not double-count: §3.3.3 has time constant
τ = -z_o/ε_o = 150 Ma and produces only ~1 km of subsidence over a
32 Ma resample window, far too slow to span the −1 to −6 km range by
itself, so the spatial zΓ must encode the subsidence history at
generation while §3.3.3 provides slow continued drift, with the
(1 − z/z_o) factor preventing the temporal rule from pushing crust
past z_o. The user decision required is whether to accept external
geophysics literature (Parsons & Sclater 1977) as authority for this
slice; if yes, the slice can be designed and implemented; if no, the
placeholder remains indefinitely.
