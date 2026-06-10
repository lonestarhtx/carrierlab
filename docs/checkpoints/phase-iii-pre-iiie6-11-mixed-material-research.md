# Phase IIIE Pre-IIIE.6.11 Mixed-Material Multi-Hit Research

Audit target: head `c16c603` (`Restore IIIE6 paper-literal zero-hit
generation`). IIIE.6.10 manual-cadence unsupported diagnosis at
`docs/checkpoints/phase-iii-slice-iiie6-10-manual-cadence-unsupported-multihit-diagnosis.md`
(in-flight, untracked) supplied the editor-default evidence this
research depends on.

This is a research-only checkpoint. It does not modify code and it
does not specify the IIIE.6.11 implementation slice. Its purpose is
to determine whether the thesis or CGF paper specifies a winner rule
for the IIIE.3 selector's `MixedMaterial` post-motion multi-hit
class so that any IIIE.6.11 design rests on accurate citation.

## Status

**paper-silent.** Both the 2020 INSA Lyon thesis and the 2019 CGF
paper are silent on a material-aware winner rule when a remesh
ray-cast finds valid post-filter intersections in plate-local
triangles whose interpolated continental fraction `x_C` differs
across the candidates. The thesis explicitly distinguishes material
types at the **convergence-detection** side (§3.3.1) but the
**divergence/remesh** side (§3.3.2.3 step 2a) lists only
subduction/collision **process-state** filtering. Neither paper
ranks plates by overall material composition for divergence-time
disambiguation.

Driftworld Tectonics is paper-extension precedent: it does encode a
continental-priority overlap rank, applied to multi-overlap
resampling. Driftworld's mechanism is plate-level vertex-count
score, not the per-candidate-triangle interpolated `x_C` test that
CarrierLab's IIIE.3 classifier already uses to populate the
`MixedMaterial` bucket. The two are structurally different even
though the directional intuition (continental wins over oceanic)
agrees.

IIIE.6.11 cannot be a paper-cited rule. Like IIIE.6.5, it must be a
named CarrierLab lab policy. Driftworld can be cited as supporting
evidence for the **direction** of the rule but not as authority for
its **per-candidate** form.

## Source 1 result — thesis re-read

Image-to-page offset: `page = image - 11`. Pages re-read directly
against the page images.

### §3.2.2 Modèle de plaque — `x_C` definition

`cc5c6807-061.png` (p.50). Tableau 3.1 lists the crust parameters.
The verbatim row for `x_C`:

> « x_C — Type de la croûte (océanique ou continentale) »

The accompanying paramétrage paragraph immediately below the table:

> « Selon le type de croûte x_C, qui peut être soit océanique soit
> continentale, nous ajoutons les paramètres suivants : l'âge de la
> croûte a_o(p) et direction locale de dorsale r(p) — et type
> d'orogénie o(p), direction locale de plissement f(p) et l'âge de
> l'orogénie a_c, qui sont par ailleurs disponibles dans le résultat
> final pour permettre son amplification (Chapitre 4). »

Three readings:

1. `x_C` is named as a **type** ("océanique ou continentale"), not a
   continuous fraction. The thesis introduces it as binary at the
   definition site.
2. The thesis does not, at this definition, name a tolerance or
   interpolation rule for `x_C` between continental and oceanic. The
   continuity of `x_C` everywhere except at init follows from the
   barycentric interpolation in §3.3.2.3 step 2a, but the thesis does
   not explicitly call that out as a rule for `x_C` in particular.
3. Consequence for IIIE.6.11: the thesis does **not** give a
   threshold (e.g., `x_C >= 0.5`) for treating a sampled point as
   "continental enough to win." That threshold, if introduced, is a
   CarrierLab choice. CarrierLab's `HasMixedIIIE3Material` already
   uses `>= 0.5`
   (`Source/CarrierLab/Private/CarrierLabVisualizationActor.cpp:1794-1810`),
   but that threshold is engineering convention, not thesis citation.

### §3.2.4 État initial — plate-level material at init

`cc5c6807-065.png` (p.54), `cc5c6807-066.png` (p.55). The plate
setup section. Verbatim from p.54:

> « Ce mode est possible via l'entrée de cartes projetées de la
> planète, qui peuvent comprendre : une carte du découpage des
> plaques, chaque plaque étant représentée par une couleur indexée;
> une carte de type bizmap (i.e., 0 ou 1), pour le type de croûte
> (en tant que portions continentales et océaniques), et une carte
> d'élévations de surface (le relief initial). »

And from p.55 random-mode:

> « Nous pouvons enfin lui [à l'utilisateur] être guidé par un ratio
> global de couverture continentale donné dans l'étape de
> spécification G ou la pondération… »

Reading: at init, `x_C` is binary 0/1 (bizmap). The plate-level
ratio of continental coverage is a user-supplied or sampled
parameter; it sets how often a plate's vertices start with `x_C = 1`
versus `x_C = 0`. Nothing in §3.2.4 establishes a plate-level rank
of plates by material composition for divergence-time use. The
thesis treats per-vertex `x_C` (binary at init) as the carrier of
material identity, not a precomputed plate-level overlap matrix.

Consequence for IIIE.6.11: the thesis does not pre-rank plates by
continental dominance. There is no thesis-cited "plate-level
continental rank" object in the model.

### §3.2 preliminaries — "principalement océanique" plates

`cc5c6807-060.png` (p.49). Background paragraph immediately before
§3.2.2:

> « Précisions. Dans les éléments préliminaires de tectonique
> classique exposés au début de ce chapitre, nous avons mentionné
> régulièrement la présence de plaques « océaniques » et de plaques
> « continentales ». En fait dans la réalité, bien que pour
> certaines plaques soient presque purement d'un type ou de l'autre,
> une plaque tectonique peut en général avoir des portions
> continentales comme des portions océaniques. C'est aussi le cas
> dans notre modèle, et c'est uniquement par abus de langage et dans
> un souci de simplification que nous parlerons de plaques océaniques
> (resp. continentales). Par convention sur le système d'îles, nous
> élargirons ce concept dans notre modèle de plaque continentale, et
> définirons un terrane comme étant une portion continentale connexe
> (au sein d'une plaque). À noter qu'avec cette définition un
> continent entier peut donc être vu comme un terrane. »

This is decisive paper context. The thesis explicitly states that a
single plate **can carry both continental and oceanic portions**.
The "oceanic" or "continental" labels are linguistic shorthand for
plate-level dominance, not a strict per-plate material class. This
matters for IIIE.6.11 because:

1. The `MixedMaterial` IIIE.3 bucket — two distinct plates, candidate
   triangles with materials on opposite sides of the `x_C >= 0.5`
   line — is **not a thesis-impossible configuration**. The thesis
   anticipates that two plates can each have both materials, and a
   sample landing near a plate boundary could read continental on
   one plate's local triangle and oceanic on another's. That is the
   exact phenomenology of the IIIE.6.10 4,301-record class at editor
   default `ContinentalPlateFraction = 0.30`.
2. The thesis's "abus de langage" framing prevents reading any
   downstream §3.3 rule as a strict per-plate material gate. If
   §3.3 had said "the continental plate wins over the oceanic plate,"
   that statement would have to mean "dominantly continental wins
   over dominantly oceanic," which is plate-level. The thesis does
   not say that anywhere on the divergence side.

### §3.3.1 Convergence — material **is** asymmetric on the convergence side

`cc5c6807-067.png` (p.56) opens §3.3.1:

> « La convergence de deux plaques peut induire une subduction, ou
> une collision continentale, en fonction de la configuration
> locale. »

`cc5c6807-067.png` continues into §3.3.1.1 Subduction:

> « Deux plaques océaniques convergentes impliquent toujours la
> subduction de la plaque la plus âgée. Une plaque océanique amorce
> toujours une subduction si elle est face à une plaque
> continentale. Enfin, nous permettons l'entame d'une obduction dans
> le cas continental-continental, quelque soit la taille des
> terranes subduits; cette obduction deviendra collision si elle est
> seulement si la masse continentale chevauchante est suffisamment
> grande. »

Reading: convergence behaviour is **explicitly asymmetric in
material type**. Three regimes are named:

- oceanic ⇄ oceanic → older subducts.
- oceanic ⇄ continental → oceanic subducts (Andéen uplift).
- continental ⇄ continental → obduction → potentially collision.

`cc5c6807-073.png` (p.62), §3.3.1.3 implementation, gives the
detection rule:

> « Pour les trois sommets du triangle T_i et si les trois sommets
> du triangle intersecté T_j sont continentaux : si tel est le cas
> nous chargeons le mode de suivi de T_i en celui de collision
> continentale et nous mémorisons l'indice j de la plaque rencontrée
> ainsi que l'indice du triangle intersecté. »

Reading: the convergence implementation tests **per-vertex** material
of both triangles to flip from subduction mode to collision mode.
This is the only place in §3.3 where the algorithm explicitly
inspects `x_C` to make a routing decision. The decision is binary
("three sommets continentaux on both triangles" → collision; else
subduction).

Consequence for IIIE.6.11: the thesis is willing to make
material-aware decisions, but only at the convergence-side. There
is **no reciprocal divergence-side material test in the thesis
implementation prose**. The asymmetry is a deliberate paper choice,
not an oversight.

### §3.3.2.1 Génération du plancher océanique — silent on multi-hit material

`cc5c6807-077.png` (p.66). The ridge ocean-generation formula:

> « Soit un point d'intérêt p situé dans la zone de divergence.
> Soient d_Γ(p) et d_P(p) les distances depuis p respectivement à la
> dorsale et à la plaque divergente la plus proche. Nous définissons
> le facteur interpolant α = d_Γ(p)/(d_Γ(p) + d_P(p)). L'élévation
> du point z(p, t + δt) dans la zone divergente est calculée par
> mélange du facteur interpolant α entre les élévations des plaques
> z̄ et un patron d'élévation représentant un profil générique de
> dorsale z_Γ (Figure 40):
>
> z(p, t + δt) = α z̄(p, t) + (1 − α) z_Γ(p, t) »

The "élévations des plaques z̄" (interpolated between plates) is
defined in the diagram of Figure 40 as the blend between **two
neighbouring plate elevations** s(c_i) and s(c_j). The text **does
not specify how those plate elevations are sampled** — it presupposes
that, given a point p in the divergence zone, the two neighbouring
plates' values are already available.

Reading: §3.3.2.1 gives the new oceanic point's elevation formula.
It does **not** condition the formula on the material types of the
two neighbouring plates. There is no clause "if the two neighbouring
plates have different material types, do X instead of Y." The text
is silent on material composition of the plates flanking the new
ridge.

Consequence for IIIE.6.11: §3.3.2.1's silence is mild support for a
"divergence-side rules don't read material" reading, but it is not
decisive — §3.3.2.1 is the post-creation elevation formula, not the
multi-hit disambiguation step. The decisive page is §3.3.2.3 step 2a
below.

### §3.3.2.3 Implémentation step 2a — silent on material winner

`cc5c6807-079.png` (p.68). The multi-valid-hit relevant text:

> « 2. Échantillonnage. Pour chaque sommet p de la TDS globale :
>
> a) On lance un rayon passant par le centre de la planète et p.
> Pour chaque plaque existante, nous testons l'intersection
> rayon-triangle via le BVH. Si l'intersection se fait avec un
> triangle en subduction ou en collision, nous ne la prenons pas en
> compte. Sinon et s'il y a intersection, nous procédons à
> l'interpolation barycentrique des paramètres de la croûte dans le
> triangle intersecté et nous les attribuons au sommet. Nous
> mémorisons pour ce sommet l'indice de la plaque intersectée. »

Five readings, each individually decisive:

1. The only explicit per-plate filter is **process-state**:
   "subduction ou collision". `x_C` is not mentioned. The text never
   says "if the intersected triangle is continental, prefer it over
   an oceanic one" or anything similar.
2. The "paramètres de la croûte" the thesis interpolates barycentrically
   in step 2a are the parameters described in §3.2.2 Tableau 3.1 —
   `x_C` (type), `e` (épaisseur), `z` (élévation), `a_o` (âge),
   `r` (direction de dorsale), `o` (orogénie type), `a_c`
   (âge orogénie), `f` (direction de plissement). `x_C` is being
   **read out** of the candidate triangle (continuous via
   barycentric), but it is not being used to **rank or filter** the
   candidate triangle.
3. The text presupposes uniqueness ("le triangle intersecté", "la
   plaque intersectée"), which is the same observation IIIE.6.5
   recorded for `cross_plate_different` and `third_plate`. The
   uniqueness presupposition fails on `MixedMaterial` for the same
   underlying reason: post-motion plate overlap creates valid hits
   in multiple plate-local triangles. Material-aware winner rule is
   not provided.
4. Step 2b (no-intersection branch) uses geometric "le plus proche"
   for q1/q2 plate-boundary partitioning. It does not invoke `x_C`
   at all. (IIIE.6.8 already noted this.)
5. Consequence: the thesis does **not** authorize a material-aware
   winner for `MixedMaterial` cases on the divergence/remesh side.
   Any such rule is a CarrierLab lab policy.

### §3.3.2.1 vs §3.3.1 — paper authority on asymmetry

This is the tightest source statement available:

| Side | Thesis `x_C`-aware? | Citation |
|---|---|---|
| Convergence detection (§3.3.1.3) | yes — per-vertex continental check switches subduction → collision | `cc5c6807-073.png` p.62 |
| Divergence ridge generation (§3.3.2.1) | no — formula does not depend on neighbouring plate material types | `cc5c6807-077.png` p.66 |
| Remesh source-triangle selection (§3.3.2.3 step 2a) | no — only subduction/collision **process-state** filter | `cc5c6807-079.png` p.68 |
| Remesh divergent-gap branch (§3.3.2.3 step 2b) | no — geometric q1/q2 only | `cc5c6807-079.png` p.68 |

The thesis's asymmetry is intentional, sustained across all three
divergence-side pages, and matches the §3.2 preliminary statement
that material identity is local-to-vertex (binary at init,
barycentric continuous afterward) rather than plate-level.

## Source 2 result — CGF paper

`docs/ProceduralTectonicPlanets/ProceduralTectonicPlanets.pdf`. The
CGF paper is more compressed than the thesis. Re-read against the
page images.

### §4.3 Oceanic crust generation — `aa42e52c-07.png`

> « The oceanic crust forms from the ridge separating diverging
> plates. As oceanic crust ages, it gets colder and denser and
> tends to lower its elevation, forming abyssal plains. »

The CGF formula z(p, t + δt) = α s(p, t) + (1 − α) z_Γ(p, t) is
identical in shape to the thesis's. CGF does **not** condition the
formula on material types of the diverging plates. Silent.

### §6 Implementation Details — `aa42e52c-08.png`

> « Plate boundary tracking. We detect plate collisions efficiently
> by tracking the intersection between the boundary triangles of
> the plates. We also rely on a bounding box hierarchy for every
> plate to accelerate intersection tests. For each tracked
> converging triangle, we also update the distance d to its nearest
> converging front and store this value by traversing the
> triangulation according to samples connectivity. »

> « Continental collisions. Terranes are handled while tracking
> plate subduction. If two continental triangles of two different
> plates intersect, we switch the tracking mode to continental
> collision. The collision event is triggered if the
> interpenetration distance between the two plates is greater than
> a user-defined threshold, 300km in our experiments. »

Reading: CGF inherits the thesis's asymmetry — material asymmetric
at convergence (subduction → collision), silent at divergence/remesh.
Adds nothing not already in the thesis prose.

### Appendix A constants — `aa42e52c-12.png`

Constants table: `δt`, `R`, `z_r`, `z_a`, `z_t`, `z_c`, `r_s`,
`Δ_c`, `r_c`, `v_a`, `v_0`, `e_o`, `e_e`, `e_a`. No constant for an
`x_C` threshold. No constant for a plate-level rank or
continental-priority parameter. Silent.

### CGF-only summary

| Question | CGF answer |
|---|---|
| Material winner at remesh multi-hit | silent |
| `x_C` threshold for continental classification | not present |
| Plate-level continental rank for overlap | not present |
| Material-aware divergence-side rule | not present |
| Material-aware convergence-side rule | yes (continental + continental → collision) |

## Source 3 result — Driftworld

`Saved/ExternalRefs/driftworld-tectonics`. Tertiary precedent, not
paper authority. Verified at the working-tree HEAD. Two relevant
loci:

### Plate score and overlap rank — `Assets/Scripts/Planet.cs:937-998`

```cpp
public int[,] CalculatePlatesVP ()
{
    int[,] retVal = new int[m_TectonicPlatesCount, m_TectonicPlatesCount];
    float[] plate_scores = new float[m_TectonicPlatesCount];
    int[]   plate_ranks  = new int[m_TectonicPlatesCount];
    List<int> ranked = new List<int>();
    for (int i = 0; i < m_TectonicPlatesCount; i++)
    {
        foreach (int it in m_TectonicPlates[i].m_PlateVertices)
        {
            // -1 if oceanic (elevation < 0), +1000 if continental.
            plate_scores[i] +=
                (m_CrustPointData[it].elevation < 0.0f ? -1 : 1000);
        }
    }
    // Rank plates by descending score; build antisymmetric overlap
    // matrix: row i, col j = +1 if i overlaps j (i is upper),
    // -1 if i goes under j.
    ...
}
```

The score is per-plate, summed over plate-vertices. A plate with
more continental vertices ranks higher. The result is an
antisymmetric `m_PlatesOverlap` matrix used everywhere that asks
"which plate is upper at this overlap."

Driftworld's documentation states the rule explicitly
(`Doc/sections/SimpleTectonicModel.tex`, §"Plate overlaps"):

> « Tectonic interactions require the concept of plate density. For
> example, denser oceanic plates are subducted under continental
> plates. Because our model does not have a clear designation of a
> plate as oceanic or continental (any plate can have oceanic or
> continental crust), we evaluate plates by a weighted sum of their
> vertices. Each plate is assigned a score equal to:
>
>   score = 100 × number of continental crust points
>         − number of oceanic crust points
>
> The plates are then ranked by the highest score. The rank decides
> which plate 'goes under' when two plates overlap (the one with a
> lower score). »

Two readings:

1. Driftworld treats continental-priority as **plate-level**,
   computed from per-vertex elevation sign aggregated across the
   whole plate. It is a single rank shared by every overlap event
   on a given plate.
2. Driftworld's documentation explicitly disclaims paper precision:
   "this ranking system is a gross simplification." Driftworld
   acknowledges it is an extension that deviates from the original
   model in its overlap mechanics.

### Resampling under multi-overlap — `Assets/Shaders/CSVertexDataInterpolation.compute:280-289` (kernel `CSCrustToData`)

```hlsl
int found_index = -1;
int found_plate = -1;

for (int i = 0; i < n_plates; i++) {
    int help_index;
    if ((found_plate == -1) ||
        (overlap_matrix[i * n_plates + found_plate] != -1)) {
        // Plate i is at least equal-or-upper to the
        // current best; only then search.
        help_index = search_plate_bvh_for_point(ivi, i);
        if (help_index != -1) {
            found_index = help_index;
            found_plate = i;
        }
    }
}
```

This kernel is the Driftworld counterpart of CarrierLab's IIIE.3
selector. When a vertex falls inside multiple plate triangles, the
loop conditions ensure that `found_plate` is updated only when the
new candidate plate ranks above (or ties with) the currently
held one. Equivalently: continental-dominant plates win over
oceanic-dominant plates at multi-overlap.

The Driftworld doc §"Oceanic crust generation & crust resampling"
restates this in prose:

> « For each initial mesh vertex, we test if it is found on a plate
> with the highest rank possible. If so, we perform barycentric
> interpolation from a triangle within which the vertex currently
> resides. If no plate is found, we create a new oceanic crust
> point. »

### Driftworld vs CarrierLab `MixedMaterial` — structural mismatch

Two structural differences between Driftworld's continental-priority
rule and CarrierLab's `MixedMaterial` bucket:

1. **Granularity.** Driftworld ranks at plate level (sum over all
   plate-vertices). CarrierLab's `HasMixedIIIE3Material`
   (`CarrierLabVisualizationActor.cpp:1794-1810`) tests **per-candidate
   triangle** by the candidate's barycentric-interpolated continental
   fraction `>= 0.5`. A plate that is overall 70% oceanic could have
   a candidate triangle whose interpolated `x_C >= 0.5` because the
   sample sits near a continental terrane region of that plate; the
   `MixedMaterial` flag fires on the per-candidate reading, not the
   plate aggregate.
2. **Score function.** Driftworld weights continental vs oceanic
   points 1000-to-1 in the per-plate sum. CarrierLab's per-candidate
   threshold is 0.5 with no asymmetric weighting. Translating
   Driftworld's rule one-to-one into CarrierLab would require a
   plate-level continental fraction signal — exactly IIIE.6.3
   Layer 1 ("higher plate-level continental fraction") — not the
   per-candidate `MixedMaterial` test.

IIIE.6.3 already lifted Driftworld's plate-level continental-priority
insight for the **same-distance shared-boundary** case. The IIIE.6.4
evidence at `ContinentalPlateFraction = 0.0` showed
`nearest_more_continental = 0` (all oceanic, no plate has an
edge); under the editor-default `ContinentalPlateFraction = 0.30`,
plate-level continental fractions do differ across plates and IIIE.6.3
Layer 1 would fire. But IIIE.6.3 fires only on `CrossPlateEqual`
shared-boundary cases — its bucket gate excludes `MixedMaterial`.

So Driftworld is **partial supporting evidence** for the **direction**
of an IIIE.6.11 rule (continental wins over oceanic at multi-overlap),
but it is **not citation** for the **per-candidate** form CarrierLab's
existing classifier semantics would naturally use.

## Source 4 result — CarrierLab IIIE.6.5 precedent

`docs/checkpoints/phase-iii-pre-iiie6-5-nearest-hit-design.md` and
`docs/checkpoints/phase-iii-slice-iiie6-5-nearest-hit-report.md`.

IIIE.6.5 explicitly declines `MixedMaterial`. The selector hook
(`CarrierLabVisualizationActor.cpp:2817-2821`):

```cpp
const bool bSupportedBucket =
    OutRecord.MultiHitBucket ==
        ECarrierLabPhaseIIIE3MultiHitBucket::CrossPlateDifferent ||
    OutRecord.MultiHitBucket ==
        ECarrierLabPhaseIIIE3MultiHitBucket::ThirdPlate;
if (!bSupportedBucket)
{
    OutRecord.NearestHitResultClass =
        ECarrierLabPhaseIIIE65NearestHitResult::UnsupportedHeld;
    return false;
}
```

The IIIE.6.5 design recorded the decline explicitly, with a probe
fixture name `mixed material - IIIE.6.5 declines, held` whose
expected result is `nearest_result = unsupported_held`. From the
IIIE.6.5 design (Recommended Rule, item 5):

> « Other buckets fall through unchanged. If the bucket is anything
> other than CrossPlateDifferent or ThirdPlate (e.g.,
> CrossPlateEqual, MixedMaterial, WithinPlateDistanceSeparated),
> IIIE.6.5 does not fire. Set NearestHitResultClass = UnsupportedHeld
> so the audit explicitly records that IIIE.6.5 considered and
> declined the case. The case continues to fall through to the
> existing classifier. »

Reading: the IIIE.6.5 decline is a CarrierLab discipline choice. The
design framed it as "narrow this slice to the buckets where IIIE.6.4
evidence is decisive (`cross_plate_different`, `third_plate`)" and
reserved the `MixedMaterial` decision for a future slice. The
decline itself is **not paper-cited**; the paper does not say
"do not resolve MixedMaterial." The decline is CarrierLab honestly
narrowing scope.

The IIIE.6.10 evidence at editor-default `ContinentalPlateFraction
= 0.30` confirms `MixedMaterial` is the dominant unsupported class:

| Scenario | Unsupported total | MixedMaterial bucket | Process-marked |
|---|---:|---:|---:|
| manual_step_16_apply_probe | 4301 | 4301 | 0 |
| manual_step_32_apply_probe | 5480 | 5480 | 0 |
| manual_step_60_selection | 8861 | 8861 | 0 |

Process-marked is zero across the entire sweep, which rules out
"these are subduction/obduction/collision triangles that the
§3.3.2.3 step-2a filter is supposed to drop." The records are
genuinely post-motion ray-cast multi-hits whose candidate triangles
disagree on `x_C >= 0.5`.

## Synthesis

### Per-question classification

| # | Question | Thesis | CGF | Driftworld |
|---|---|---|---|---|
| 0 | Primary: material-aware winner rule for `MixedMaterial` post-motion multi-hit | **silent** (§3.3.2.3 step 2a only filters process-state) | **silent** (§4.3, §6) | **partial support, structural mismatch** (plate-level rank, not per-candidate) |
| 1 | Does §3.3.2.3 reference `x_C` for source-triangle selection? | **silent** — `x_C` is interpolated **out of** the chosen triangle, not used to choose it | n/a (CGF inherits) | n/a |
| 1b | Does any §3.3.2 page distinguish material types in ray-cast or selector logic? | **silent** at §3.3.2.1 ridge formula, §3.3.2.3 step 2a remesh, §3.3.2.3 step 2b q1/q2 | **silent** | n/a |
| 2 | Symmetric material-aware rule on §3.3.2 (divergence) side? | **no, deliberately** — §3.3.1 has explicit material rules at convergence (subduction vs collision); §3.3.2 has none | inherits the asymmetry | not applicable (Driftworld treats overlap globally, not separated by convergence/divergence) |
| 3 | Overlap rank between plate types (continental-dominant vs oceanic-dominant)? | **no plate-level rank object exists** in the model | no | **yes** (`Planet.cs:937` `CalculatePlatesVP`, `m_PlatesOverlap`) — but plate-level, not per-candidate |
| 4 | Is the IIIE.6.5 "decline mixed-material" pattern paper-cited? | **no** — paper does not specify "do not resolve these"; decline is CarrierLab discipline | no | not applicable |
| 5 | Driftworld mixed-material analog applicable to CarrierLab `MixedMaterial`? | n/a | n/a | **directional support only** — Driftworld establishes "continental wins over oceanic at multi-overlap" but via plate-level vertex count, not per-candidate `x_C >= 0.5` test |

### Categorical summary

- **Thesis-explicit rule for IIIE.6.11**: none.
- **Thesis-implicit rule that could be stretched to cover IIIE.6.11**: the §3.3.1 convergence convention "continental supersedes oceanic" is a plate-level per-vertex test that fires only at convergence detection; the thesis does not extend it to divergence-side multi-hit. Stretching it to divergence is an extension, not an application.
- **Thesis-silent**: the divergence-side multi-hit material winner. This is the controlling status.
- **Driftworld-supported (with structural caveat)**: the **direction** "continental wins over oceanic at multi-overlap" is Driftworld-cited. The **mechanism** (plate-level rank vs per-candidate `x_C` test) is structurally different from CarrierLab's existing classifier.
- **CarrierLab-invented**:
  - The `>= 0.5` threshold for `HasMixedIIIE3Material`.
  - The IIIE.6.5 `UnsupportedHeld` decline of `MixedMaterial`.
  - Any IIIE.6.11 winner rule built on per-candidate continental fraction.

## Implication for IIIE.6.11

IIIE.6.11 is **"design named lab policy because thesis is silent and
Driftworld provides directional but structurally different
precedent."** It is the same disposition class as IIIE.6.5
(post-motion nearest-hit) and IIIE.6.8 (non-separating boundary): a
named CarrierLab lab policy, disclosed as such, not as a paper-cited
rule.

The citation chain for the disclosure:

1. Thesis §3.3.2.3 step 2a (`cc5c6807-079.png`, p.68) presupposes
   uniqueness of the per-sample plate intersection without naming a
   winner; only the subduction/collision **process-state** filter
   is explicit. Material type `x_C` is not in the filter chain.
2. Thesis §3.3.2.1 (`cc5c6807-077.png`, p.66) and §3.3.2.3 step 2b
   (`cc5c6807-079.png`, p.68) do not condition divergence-side
   computation on neighbouring-plate material composition.
3. Thesis §3.3.1.3 (`cc5c6807-073.png`, p.62) is material-aware at
   convergence (per-vertex continental check switches subduction →
   collision) and is the only place the thesis exercises a
   material-type routing decision.
4. CGF §4.3 and §6 inherit the thesis's asymmetry. No additional
   rule.
5. Driftworld `Planet.cs:937` `CalculatePlatesVP()` and
   `CSVertexDataInterpolation.compute:280-289` `CSCrustToData`
   resolve multi-overlap by plate-level continental rank. This is
   directional precedent for "continental wins" but at a different
   granularity than CarrierLab's per-candidate `MixedMaterial`
   classifier.
6. CarrierLab IIIE.6.5 declined `MixedMaterial` to narrow scope;
   the decline itself is CarrierLab discipline, not paper-cited.

The disclosure language for IIIE.6.11 must carry items 1–6 verbatim,
mirror the IIIE.6.5 disclosure tone ("This is not a paper-cited rule
and is not a claim of paper-faithfulness"), and explicitly state
which granularity (plate-level rank vs per-candidate fraction) the
slice chooses, with the reason.

## Recommended rule shape (preliminary)

This is a research checkpoint, not a design checkpoint. The
recommendation is preliminary and is for the user to redirect as
desired before any IIIE.6.11 design starts.

The recommended shape, in one paragraph: a **per-candidate
continental-priority lab policy**, fired only on the `MixedMaterial`
bucket (one or more candidates with `x_C >= 0.5` and one or more
with `x_C < 0.5`), in which the candidate with the higher
barycentric-interpolated `ContinentalFraction` wins; ties on
`ContinentalFraction` (within a small tolerance) hold fail-loud, do
**not** fall through to IIIE.6.5 nearest-hit, IIIE.6.3
shared-boundary, or lower-plate-id. The rule is mutually exclusive
with IIIE.6.5 (which declines `MixedMaterial`) and IIIE.6.3 (whose
bucket gate excludes `MixedMaterial`). Citation: Driftworld
(directional support, plate-level mechanism, `Planet.cs:937`,
`CSVertexDataInterpolation.compute:280-289`); thesis §3.3.1.3
material-aware **convergence-side** rule (`cc5c6807-073.png`, p.62)
as analogous internal precedent for material-type routing decisions
in the same paper. The slice must disclose explicitly that the
**divergence-side** rule is not paper-cited (thesis §3.3.2.3 step
2a, `cc5c6807-079.png`, p.68) and that the per-candidate
granularity is a CarrierLab choice rather than Driftworld's
plate-level form.

This shape is one of several defensible options. The primary
alternative is a **plate-level continental-priority** rule, which
would more closely cite Driftworld's mechanism and would re-use the
plate-level continental-fraction signal already wired through
IIIE.6.3 Layer 1. That alternative trades direct Driftworld citation
for a divergence between the IIIE.3 classifier (per-candidate) and
the IIIE.6.11 resolver (plate-level). It is included in the open
questions below.

## Open questions

1. **Per-candidate vs plate-level continental priority.** The
   recommendation above is per-candidate (matches the existing
   `HasMixedIIIE3Material` granularity). The alternative is
   plate-level (matches Driftworld and IIIE.6.3 Layer 1). User
   decision sets the IIIE.6.11 design direction. **User decision
   needed before IIIE.6.11 design starts.**
2. **Default-on vs default-off.** Per the user's standing memory
   "New feedback loops default off" (2026-04-30 sub-phase
   convention), and given IIIE.6.11 introduces a **new
   material-aware authoritative-state classifier**, the
   recommendation is **default-off**. IIIE.6.5 was default-on
   because nearest-distance is not new authoritative-state feedback;
   IIIE.6.11 introduces an `x_C`-derived routing decision into the
   IIIE.3 selector, which is closer to a new feedback edge. **User
   decision welcome.**
3. **Tie tolerance for `ContinentalFraction`.** If the rule is
   per-candidate, the tolerance for "ties" is a parameter. The
   IIIE.3 family already uses `1.0e-9` for dimensionless aggregates
   and `1.0e-9 km` for geometric distances; consistency suggests
   `1.0e-9` for `ContinentalFraction` as well. This is a slice
   design decision, not a research question, but it affects the
   open-question count.
4. **Distance-tie composability.** If a `MixedMaterial` case also
   has equal continental fractions (continental-tie), should the
   resolver fall through to nearest-hit (IIIE.6.5)? IIIE.6.5
   currently declines `MixedMaterial`; if IIIE.6.11 lifts that
   decline, the question is whether nearest-hit should fire as a
   second-stage tiebreak or whether `MixedMaterial` ties hold
   fail-loud. Recommendation per IIIE.6.5 precedent: hold
   fail-loud, no fallthrough across distinct geometric classes.
5. **Process-state cross-reference.** IIIE.6.10 measured
   `process-marked = 0` across all unsupported records, ruling out
   the upstream IIIB/IIIC/IIID marking redirect. Confirmed by
   independent diagnosis. Not an open question, but worth noting:
   the `MixedMaterial` records are genuine post-motion
   multi-hits, not unmarked subduction/collision triangles.
6. **Hash-regression strategy.** IIIE.6.11 will introduce new
   per-record audit fields (e.g., `bUsedMixedMaterialPolicy`,
   `MixedMaterialResultClass`, `WinnerContinentalFraction`). Per
   IIIE.6.5 precedent, schema-additive hashes will drift on the
   IIIE.6.4 / IIIE.6.5 / IIIE.6.6 baseline replays even with the
   new resolver disabled, and the slice report must publish both
   old and new baseline hashes with a clear "schema addition" note.
   Not a research question, but unavoidable for the design slice.
7. **Mutual exclusion with IIIE.6.5 / IIIE.6.6 / IIIE.6.3.** All
   four resolvers handle structurally distinct geometric classes
   (`MixedMaterial`, `CrossPlateDifferent`+`ThirdPlate`,
   distance-tie subset, `CrossPlateEqual` shared-boundary
   respectively). The audit invariant (no record co-fires two
   resolvers) carries over from IIIE.6.5 unchanged. Stop-condition
   FATAL on co-fire.

## Summary for independent review

This research checkpoint resolves a single narrow question: when a
remesh ray-cast at a post-motion zero-hit divergent gap finds valid
post-filter intersections in plate-local triangles whose
barycentric-interpolated continental fraction `x_C` differs across
the candidates (one ≥ 0.5, another < 0.5), what does the
2020 INSA Lyon thesis or the 2019 CGF paper specify? The answer,
verified by direct re-read of `cc5c6807-060.png` through
`cc5c6807-079.png` and `aa42e52c-07.png` through `aa42e52c-12.png`,
is **paper-silent on the divergence-side material winner**. The
thesis is willing to make material-aware routing decisions on the
**convergence side**: §3.3.1.3 (`cc5c6807-073.png`, p.62) explicitly
tests per-vertex continental status of two intersected triangles to
flip from subduction mode to collision mode. The thesis is **not**
willing to make the analogous decision on the **divergence side**:
§3.3.2.3 step 2a (`cc5c6807-079.png`, p.68) lists only the
subduction/collision **process-state** filter when iterating over
plates for ray-triangle intersection, and presupposes uniqueness of
the chosen plate without naming a material-aware winner; §3.3.2.1
(`cc5c6807-077.png`, p.66) ridge-elevation formula does not
condition on neighbouring-plate material composition; §3.3.2.3 step
2b q1/q2 partitioning is geometric only. CGF §4.3 and §6
(`aa42e52c-07.png`, `aa42e52c-08.png`) inherit the thesis's
asymmetry without elaboration. Driftworld Tectonics has the closest
precedent: `Planet.cs:937` `CalculatePlatesVP()` builds per-plate
scores `+1000 if continental, -1 if oceanic` per crust vertex,
ranks plates, and writes an antisymmetric overlap matrix used by
`CSVertexDataInterpolation.compute:280-289` `CSCrustToData` so that
continental-dominant plates win at multi-overlap. Driftworld is
**directional** support for IIIE.6.11 (continental wins over
oceanic) but is **structurally different** in granularity:
Driftworld's rank is plate-level vertex count, while CarrierLab's
`HasMixedIIIE3Material` (`CarrierLabVisualizationActor.cpp:1794-1810`)
flags multi-hits by per-candidate-triangle interpolated
`ContinentalFraction >= 0.5`. The IIIE.6.5 design's decline of
`MixedMaterial` to `UnsupportedHeld`
(`CarrierLabVisualizationActor.cpp:2817-2821`, with the named
fixture `mixed material - IIIE.6.5 declines, held`) is itself
CarrierLab scope discipline, not paper-cited; the paper does not
say "do not resolve these." The IIIE.6.10 evidence at editor-default
`ContinentalPlateFraction = 0.30` confirms `MixedMaterial` is the
dominant unsupported class (4,301 records at step 16 growing to
8,861 at step 60), and `process-marked = 0` across the sweep rules
out the alternate hypothesis that these are unmarked
subduction/obduction/collision triangles the §3.3.2.3 step-2a
filter should have dropped. Implication for IIIE.6.11: the slice
**cannot** claim paper authority and must be a named CarrierLab lab
policy, disclosed as such, mirroring IIIE.6.5's discipline. The
preliminary recommended rule shape is a **per-candidate
continental-priority** policy (winner = candidate with higher
interpolated `ContinentalFraction`, ties hold fail-loud, no
fallthrough to IIIE.6.5/IIIE.6.3/lower-plate-id) with Driftworld
cited as directional support and the per-candidate granularity
explicitly disclosed as a CarrierLab choice rather than Driftworld's
plate-level form. The two open questions for the user before any
IIIE.6.11 design starts are **per-candidate vs plate-level
granularity** (this research recommends per-candidate, but the
plate-level alternative cites Driftworld more directly and re-uses
the IIIE.6.3 Layer 1 signal), and **default-off vs default-on**
(this research recommends default-off per the standing
"new feedback loops default off" memory, since IIIE.6.11
introduces a new `x_C`-derived routing decision into the IIIE.3
selector). All other questions (tie tolerance, distance-tie
composability, hash-regression strategy, mutual exclusion with the
other IIIE.6 resolvers) are scope-internal slice-design decisions
that follow from the IIIE.6.5 precedent.
