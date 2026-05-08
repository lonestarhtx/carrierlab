# Phase IIIE.6.3 Multi-Hit Tie-Break Design

Audit target: head `550fcd5` (`Verify IIIE boundary-hit architecture`); IIIE.6.2
diagnostic landed at `2684d95`; IIIE.6.1 within-plate coalescing at `cebf593`.

This is a planning-only checkpoint. It does not modify code. It specifies the
shape of the next IIIE.6.3 implementation slice for Codex execution: a single
named lab policy that resolves the 5,845 default-scale boundary-shared
multi-hit holds the IIIE.3 selector currently fails loud on.

## Status

DESIGN READY. Implementation packet below is sufficient for a bounded Codex
slice. One axis (older-vs-younger oceanic age direction at Layer 2) is the
weakest defensibility argument and is flagged in Open Questions; the user
may want to redirect before Codex starts. Otherwise the slice is ready.

Post-draft addendum: the "Driftworld secondary implementation precedent"
section below was added after the original planning draft. It does not change
the thesis/CGF verdict. It does refine the recommended Layer 1 rule: where
the original draft says "exactly one candidate plate has continental fraction
>= 0.5", Codex should prefer the higher plate-level continental fraction
within an explicit epsilon, then fall through to older oceanic age and lower
plate id only if the plate-level continental fractions tie. This addendum is
an amendment to the original design, not a replacement of the source review.

## Source 1-2 result — thesis / CGF re-read

The architectural verification (`docs/checkpoints/phase-iii-architectural-deviation-verification.md`)
already documented that the thesis and CGF paper are silent on the multi-valid
same-distance ray-cast tie-break. This re-read confirms that finding by
direct quotation against the page images and adds the explicit observation
that the paper-side language is consistently singular wherever it could have
named a tie-break.

### Thesis §3.2.4 plate construction (`cc5c6807-066.png`, p.55)

> « Quel que soit le mode de spécification, utilisateur ou automatique, il
> s'agit ensuite de construire la représentation numérique des plaques.
> Partant de la triangulation de Delaunay sphérique globale pré-calculée,
> nous extrayons des sous-triangulations, une pour chaque plaque (Figure
> 35). Pour ce faire nous repérons et recopions les triangles et sommets
> associés de la TDS globale dans des triangulations sphériques dédiées.
> Ces opérations de création de plaques impliquent de **dupliquer**, de
> **ré-indicer** et de **re-compacter** les tableaux stockant la topologie
> de la TDS globale, tout en ajoutant le voisinage vide aux frontières des
> plaques. Au final, et étant donné N plaques partitionnant la croûte,
> nous instancions N triangulations de Delaunay sphériques, qui serviront
> de support numérique au modèle. »

The verb sequence dupliquer / ré-indicer / re-compacter is the exact
architecture CarrierLab implements via `BuildPlateLocalTriangulations`. The
text says boundary triangles inherit shared global TDS vertices through
duplication; it does not name a winner if a later ray-cast reaches both
duplicated copies at the same distance.

### Thesis §3.3.2.3 remesh ray-cast, step 2a (`cc5c6807-079.png`, p.68)

> « 2. Échantillonnage. Pour chaque sommet p de la TDS globale :
>
> a) On lance un rayon passant par le centre de la planète et p. Pour
> chaque plaque existante, nous testons l'intersection rayon-triangle via
> le BVH. Si l'intersection se fait avec **un triangle en subduction ou
> en collision**, nous ne la prenons pas en compte. Sinon et **s'il y a
> intersection**, nous procédons à l'**interpolation barycentrique** des
> paramètres de la croûte dans le triangle intersecté et nous les
> attribuons au sommet. Nous mémorisons pour ce sommet **l'indice de la
> plaque intersectée**. »

Three observations:

1. The plate iteration is plural ("Pour chaque plaque existante, nous
   testons"), but the consequence is singular ("le triangle intersecté",
   "la plaque intersectée"). The text presupposes uniqueness without
   asserting it.
2. The only explicit per-plate filter is subduction/collision invisibility.
   No "first plate", "closest plate", "previous owner", "lower id", or
   "primary plate" rule is anywhere in step 2.
3. The "indice de la plaque intersectée" phrase is the assignment artifact
   that step 3 partitions on. It is not a tie-break; if step 2a is silent
   on multi-valid hits, step 3 inherits whatever step 2a writes.

Step 2b (no-hit divergent-gap branch) and step 3 (partition + duplicate +
re-index for new plate triangulations) are both downstream of step 2a's
unique-index assumption and add nothing new for the boundary-shared case.

### Bibliography cross-check (Source 7 finding)

Two thesis bibliography entries cover computational-geometry primitives that
might in principle anchor a half-open / simulation-of-simplicity convention:

- **CDS12** = Cheng, Dey, Shewchuk, *Delaunay Mesh Generation*, CRC Press
  2012 (`cc5c6807-127.png`, bibliography p.116). Cited at thesis p.53,
  i.e. inside the §3.2 plate-construction discussion of "algorithmes
  classiques de construction de triangulations de Delaunay euclidiennes".
- **Ren97** = R. J. Renka, "Algorithm 772: STRIPACK: Delaunay triangulation
  and Voronoi diagram on the surface of a sphere", *ACM TOMS* 23.3
  (1997), 416–434 (`cc5c6807-133.png`, bibliography p.122). Cited at
  thesis p.52, again only for spherical-triangulation construction.

Neither reference is cited from §3.3.2.3 or anywhere in Chapter 3's
remeshing prose. The thesis used Cheng/Dey/Shewchuk-style and Renka-style
methods at construction time but did not import any of their degeneracy or
boundary-ownership conventions into the ray-cast remesh step. **There is no
basis in the thesis or its cited literature for claiming that a CGAL-style
half-open convention or Edelsbrunner–Mücke simulation-of-simplicity rule
was implicitly inherited.** Any such claim in IIIE.6.3 would be invented
faithfulness.

### CGF 2019 paper §4 / §6

Confirmed against `aa42e52c-04.png`, `aa42e52c-05.png`, `aa42e52c-08.png`.
Section 4 names plates as spherical triangulations carrying the Table 1
crustal/tectonic parameters (`x_C`, `c`, `z`, `a_o`, `f`, `a_c`, `o`, `x`).
Section 6 says implementation builds a "global Spherical Delaunay
Triangulation [BHJSP07]" partitioned into plates and re-used at remesh, and
that non-divergent samples receive parameters by barycentric interpolation
from "the plate they intersect". The CGF text is more compressed than the
thesis and does not add a multi-valid tie-break the thesis does not
already provide.

### Net result

Sources support exactly the architectural verdict in
`docs/checkpoints/phase-iii-architectural-deviation-verification.md`: the
boundary-shared multi-valid same-distance case is a real geometric
possibility of the duplicated-per-plate-from-global-TDS architecture both
projects use, and neither the thesis prose nor its cited literature names a
winner rule for it. **IIIE.6.3 cannot be a paper-cited rule. It must be a
named lab policy. It will never be paper-faithful.** That is not a defect;
it is the only honest framing.

## Addendum - Driftworld secondary implementation precedent

This addendum was added after the original draft at the user's request. It
uses Driftworld Tectonics as **secondary implementation precedent** only. It
does **not** promote Driftworld to paper authority and does **not** turn the
IIIE.6.3 rule into a paper-cited rule.

### Source and authority level

Driftworld Tectonics (`github.com/hecubah/driftworld-tectonics`, inspected at
HEAD `b3e21c6b6a64b21e0a739cb8ab4ce66d319546d0`) is explicitly inspired by
Cortial et al. The repository README says Driftworld "utilizes ideas and
methods described in an article by Yann Cortial et al. in 2019", and its
documentation acknowledgements say discussions with Yann Cortial helped the
author decide Driftworld's scope and form. That makes Driftworld a useful
implementation precedent, especially because it independently had to choose
practical policies around overlapping plate data.

Authority boundary: Driftworld also states that it simplifies Cortial's model.
Its overlap/rank behavior is therefore evidence that a continental-priority
policy is engineering-defensible, **not** evidence that the thesis or CGF
paper specified the same policy.

### Driftworld rule shape

Driftworld documentation section 3.5 ("Plate overlaps") ranks plates by a
continental versus oceanic score:

```text
score = 100 x number of continental crust points - number of oceanic crust points
```

The same section says the rank decides which plate "goes under" when two
plates overlap and gives as an example the case where the simulation must know
which overlapping plate defines a crust point elevation on the surface.

Driftworld documentation section 3.7 ("Oceanic crust generation & crust
resampling") then applies that rank at resampling time: for each original mesh
vertex, Driftworld tests whether the vertex is found on a plate with the
highest rank possible; if so, it barycentrically interpolates crust data from
that triangle; if no plate is found, it creates a new oceanic crust point.

The code matches the prose. In
`Assets/Shaders/CSVertexDataInterpolation.compute`, `CSCrustToData` loops all
plates and updates `found_index` / `found_plate` only when the candidate plate
is allowed by `overlap_matrix[i * n_plates + found_plate] != -1`. In
`Assets/Scripts/Planet.cs`, `CalculatePlatesVP()` builds that matrix from
plate scores where continental vertices dominate oceanic vertices. In short:
Driftworld does not pick the first hit or the previous owner; it chooses the
ranked-upper plate, and that rank is driven primarily by continental material.

### Consequence for CarrierLab IIIE.6.3

Driftworld strongly supports **continental priority** as the first layer of
CarrierLab's approved lab-policy tie-break. It does not support the proposed
Layer 2 older-oceanic-age rule; Driftworld's second-order behavior is its own
rank/score/order, not an oceanic-age comparison.

The stronger CarrierLab amendment is therefore:

1. **Layer 1 - Higher plate-level continental fraction.** Compare
   `ComputePlateContinentalFraction(State, PlateId)` for all candidate plates.
   If one candidate exceeds all others by epsilon, it wins. Record
   `SharedBoundaryTieBreakRule = ContinentalPriority` and record the winning
   margin in the audit evidence. This avoids a hard cliff at `0.5`: a plate
   that is `0.49` continental should beat a plate that is `0.02` continental
   before the resolver falls through to oceanic age.
2. **Layer 2 - Older plate-level oceanic age.** If Layer 1 ties within
   epsilon, use the original draft's older-oceanic-age rule. This remains a
   CarrierLab lab-policy choice, not a Driftworld precedent.
3. **Layer 3 - Lower plate id.** If Layer 2 ties within tolerance, use the
   original deterministic final fallback.

Recommended Layer 1 tolerance: `1.0e-9` absolute for the dimensionless
continental-fraction aggregate, matching the tolerance family already used by
IIIE.4 independent-oracle scalar checks and IIIE.6.1 multi-hit equivalence
checks.

### Audit amendment

The original audit-by-construction packet still applies, with one clarification:
`SharedBoundaryTieBreakRule = ContinentalPriority` should mean "higher
plate-level continental fraction won", not only "exactly one candidate was
above a continental threshold." The per-record audit fields already proposed
in the original draft are sufficient because they record all candidate
continental fractions. The slice report should add a Driftworld-precedent row
to the rule table:

| Evidence | Required wording |
|---|---|
| Driftworld precedent | "Driftworld independently uses continental-dominant plate rank to choose the top overlapping plate during resampling. CarrierLab treats this as secondary implementation precedent for the approved lab policy, not as thesis or CGF authority." |
| Layer 1 definition | "ContinentalPriority = higher plate-level continental fraction by epsilon; threshold-only continental status is not the resolver." |
| Layer 2 limitation | "Driftworld does not justify older-oceanic-age; Layer 2 remains a CarrierLab deterministic lab-policy choice." |

This addendum makes the recommended rule slightly less arbitrary while keeping
the honesty boundary intact: paper silent, Driftworld informative, CarrierLab
policy disclosed.

## Tie-break candidate analysis

Each candidate is weighed for: defensibility, simplicity, determinism,
physical justification, and edge cases. Distinct judgments per shape class
are flagged where they apply.

### Field availability and the candidate-vs-plate-level distinction

IIIE.6.2 measured zero `TwoPlateFieldMismatch` and zero
`ThreePlateEdgePlusIntruder` cases at default 100k/40 seed 42, against 5,845
unresolved holds. The implication is decisive: for the holds the live cadence
actually hits, the per-candidate barycentrically-interpolated `OceanicAge`,
`ContinentalFraction`, and `Elevation` values are equal across candidates
within IIIE.3 tolerance (because the candidates are duplicated-from-the-
same-global-TDS-vertex cases). Any field-based rule that consults
candidate-interpolated fields will tie at the same value and resolve nothing.

A field-based rule must therefore use **plate-level aggregates**, not
candidate-interpolated fields. CarrierLab already exposes:

- `ComputePlateContinentalFraction(State, PlateId)` at
  `Source/CarrierLab/Private/CarrierLabVisualizationActor.cpp:724`. Returns
  the area-weighted mean of `Vertex.ContinentalFraction` over `Plate.Vertices`,
  falling back to `Plate.bContinental ? 1.0 : 0.0` if total weight is zero.
- `ComputePlateOceanicAge(State, PlateId)` at
  `:750`. Returns the area-weighted mean of `Vertex.OceanicAge` over
  `Plate.Vertices`, zero if total weight is zero.

Both helpers are already consumed by `FConvergenceSubductionPolarityDecision`
(see `:11637-11640`). Reusing them in IIIE.6.3 means the new tie-break
shares its provenance with IIIB's existing polarity decisions.

### Candidate 1 — Lower plate ID

| Axis | Verdict |
|---|---|
| Defensibility | None physical. Setup-order arbitrary (Voronoi seed assignment determines numbering). |
| Simplicity | Highest. One integer comparison. |
| Determinism | Total. No tolerance, no field, no aggregation. |
| Physical justification | None. |
| Edge cases | None — every pair of plates has distinct integer IDs. |

Strength: provably terminating, never ambiguous, trivial to fixture. Weakness:
zero geological content; biases the simulation toward whichever plates the
Voronoi seed picked first. Honest as a final fallback layer; weak as a sole
rule.

### Candidate 2 — Older oceanic age (plate-level)

| Axis | Verdict |
|---|---|
| Defensibility | Moderate. Older oceanic crust is denser and more established; younger is being created at active spreading ridges. Same field IIIB uses. |
| Simplicity | Moderate. Reuses `ComputePlateOceanicAge`. |
| Determinism | Strong but tolerance-sensitive. Two plates' weighted-mean ages can be near-equal at high precision. |
| Physical justification | Real but weak as a *boundary-ownership* rule. The OceanicAge field is thesis-cited (Table 3.2). The geophysics around it (older subducts under younger) is about IIIB polarity, not about IIIE remesh ownership. |
| Edge cases | Continental plates have undefined oceanic age (mean is dominated by a few oceanic vertices on continental shelves, or zero). Direction (older-wins vs. younger-wins) is essentially a coin flip on physics; older-wins is more consistent with IIIB code reuse. |

Strength: thesis-cited field. Weakness: directionality is arguable. Layer-2
fit only.

### Candidate 3 — Larger plate area

| Axis | Verdict |
|---|---|
| Defensibility | Moderate. Larger plates are more established, slower, less likely to be in active rifting. |
| Simplicity | Moderate. Sum `Vertex.AreaWeight` over `Plate.Vertices`. No new field needed. |
| Determinism | Strong but tolerance-sensitive. Two plates' weighted areas can be near-equal at high precision. |
| Physical justification | Defensible but generic. |
| Edge cases | Continental supercontinents grow by absorbing boundaries, so this rule biases toward supercontinent formation. Possibly not desirable for paper-fidelity over long horizons. |

Strength: simple, deterministic, plausible. Weakness: long-horizon bias
toward over-aggregation. Layer-2 alternative if older-oceanic-age is rejected.

### Candidate 4 — Continental fraction priority (plate-level)

| Axis | Verdict |
|---|---|
| Defensibility | High. Continental material is preserved across remesh; oceanic material is regenerated. Aligns with the IIIE.6 rifting-pending discipline. |
| Simplicity | High. `ComputePlateContinentalFraction(...) >= 0.5` already in use. |
| Determinism | Total when exactly one plate is continental. Ties when both have the same continental status. |
| Physical justification | Strong. The IIIE.6 lab policy already commits to never overwriting continental material with divergent oceanic generation; preferring continental ownership at boundary ambiguities is the same discipline applied to selection rather than generation. |
| Edge cases | Ocean-ocean: ties (both `< 0.5`). Continent-continent: ties (both `>= 0.5`). Both cases need a Layer 2. |

Strength: physically and policy-coherently motivated. Weakness: only resolves
the asymmetric case, not ocean-ocean or continent-continent. Best fit for
**Layer 1 of a hierarchy.**

### Candidate 5 — Lower elevation at boundary (candidate-interpolated)

Tied for the same reason as candidate-interpolated fields generally tie:
the IIIE.6.2 distribution shows zero field-mismatch holds, so candidate
elevations are equal across candidates at boundary-shared cases. **Will
not break the live-cadence ties.** Discard.

### Candidate 6 — Hybrid / hierarchical

Most defensible. Layered candidates above into a single deterministic order.
The hierarchy this checkpoint recommends is:

1. **Continental priority** (Candidate 4): if exactly one plate has
   plate-level mean continental fraction `>= 0.5`, that plate wins.
2. **Older oceanic age** (Candidate 2, older-wins direction): among plates
   that tie on Layer 1 (both continental or both oceanic), the plate with
   greater plate-level mean OceanicAge wins, with a tolerance ε to absorb
   weighted-mean float precision noise.
3. **Lower plate ID** (Candidate 1): final deterministic fallback.

### Per-class differences

Edge / vertex-only / three-plate cases share the same physical question:
which plate "owns" a global TDS feature that two or three plate-local
triangulations duplicated. There is no thesis-side hint that the answer
should differ by class. **The same rule applies to all three classes**, and
the audit distinguishes the classes only via the per-record shape enum so
the report can confirm distribution against IIIE.6.2's measured 3,740 /
2,021 / 84 split.

Three-plate cases need a single sample-level winner (see "Three-plate
handling" below). The user's correction note is decisive: triangle-level
centroid-split (IIIE.5) does not extend to sample-level partition because
each global TDS vertex carries one plate index in step 3 of §3.3.2.3 page
69. A sample-level split would require a step-3 architectural change. The
hierarchical pairwise-elimination rule produces a single winner and matches
the existing single-index-per-vertex contract.

## Recommended rule

### Single hierarchical rule, applied uniformly across all three resolved classes

For each unresolved hold whose IIIE.6.2 shape is in
`{ TwoPlateSharedGlobalEdge, TwoPlateSharedGlobalVertexOnly, ThreePlateCommonGlobalVertex }`:

1. Compute plate-level aggregates for each candidate plate via the existing
   `ComputePlateContinentalFraction(State, PlateId)` and
   `ComputePlateOceanicAge(State, PlateId)`.
2. **Layer 1 — Continental Priority.** Let `IsContinental(P) :=
   ComputePlateContinentalFraction(State, P) >= 0.5` (the same threshold
   `IsPlateLocalTriangleContinental` uses at line 800). If exactly one
   candidate plate is continental, that plate wins. Set
   `SharedBoundaryTieBreakRule = ContinentalPriority`.
3. **Layer 2 — Older Oceanic Age.** If Layer 1 ties (all continental or
   all oceanic), compute `Age(P) := ComputePlateOceanicAge(State, P)` for
   each candidate. The plate with maximum age wins, where "maximum" uses
   strict greater-than with a tolerance ε = 1.0e-6 Ma, matching the IIIB.5
   `FMath::IsNearlyEqual(..., 1.0e-6)` convention at
   `CarrierLabPhaseIIIB5Commandlet.cpp:356-357`. If exactly one plate
   strictly exceeds the others, it wins. Set
   `SharedBoundaryTieBreakRule = OlderOceanicAge`.
4. **Layer 3 — Lower Plate ID.** If Layer 2 ties within ε, the plate with
   the smallest `PlateId` wins. Set
   `SharedBoundaryTieBreakRule = LowerPlateId`.

### Three-plate handling

For shape `ThreePlateCommonGlobalVertex` with three candidate plates
{P0, P1, P2}: apply the hierarchy as a **single-elimination tournament**:

- Compare `Resolve(P0, P1)` to produce winner `W01`.
- Compare `Resolve(W01, P2)` to produce winner `W`.
- `W` is the sample-level owner.

The rule is associative under the strict-tolerance comparisons above (Layer
1 boolean is associative, Layer 2 with a single threshold is associative
when ε is symmetric, Layer 3 is integer order). The audit must record
`SharedBoundaryShapeClass = ThreePlateCommonGlobalVertex` and
`SharedBoundaryTieBreakRule` from the layer that produced the final winner
W (not the intermediate W01).

Architectural justification for single-owner over sample-level split:
§3.3.2.3 step 3 (page 69) reads "nous nous servons des assignations
précédentes d'indices de plaque, **que nous avons attribués à chaque
sommet** p de la TDS globale" — singular index per global vertex. CarrierLab's
`FCarrierLabPhaseIIIE5RemeshVertexRecord.AssignedPlateId` is a single int.
A sample-level split would require a step-3 architectural change beyond
this slice's scope and is therefore explicitly out of bounds. IIIE.5's
centroid-split topology operates on mixed global TDS triangles after step
3 has assigned vertices; it is not a precedent for splitting a vertex
itself.

### Held classes (rule does not apply, fail-loud preserved)

These shapes must remain unresolved and counted in
`UnresolvedMultiHitCount` so the live cadence still fails loud on them:

- `TwoPlateFieldMismatch` — barycentric fields disagree across candidates,
  so collapsing to one plate would silently discard physical evidence the
  rule cannot honestly arbitrate. (IIIE.6.2 default: 0; gate fixture
  required.)
- `TwoPlateNoSharedGlobalVertices` — two coincident hits without a shared
  global TDS feature is a different geometric class than a duplicated
  boundary; not the case the rule was approved for. (IIIE.6.2 default: 0;
  gate fixture required.)
- `ThreePlateEdgePlusIntruder` — a third plate that does not share a
  common vertex with the other two is not a triple-junction; out of scope.
  (IIIE.6.2 default: 0; gate fixture required.)
- `ThreePlateNoCommonSourceVertex` — three plates with no common global
  vertex is structurally inconsistent with the duplicated-from-shared-TDS
  architecture and should remain a stop condition. (IIIE.6.2 default: 0.)
- `NonBoundaryOrInteriorOverlap` — interior-of-triangle overlaps are not
  boundary ambiguities. (IIIE.6.2 default: 0; gate fixture required.)
- `InvalidOrUnclassified` — degenerate shape; must hold. (IIIE.6.2
  default: 0.)

### Justification summary (engineering-defensibility, not paper-faithfulness)

Why this hierarchy survives challenge:

1. **Layer 1 leverages an existing IIIE lab discipline.** The IIIE.6
   rifting-pending route already commits CarrierLab to never overwriting
   continental material with newly generated oceanic crust. A boundary
   tie-break that prefers the continental plate is the same discipline
   applied to selection. The two policies harmonize; the consolidation
   can describe both as expressions of "continental material persists
   across remesh".
2. **Layer 2 reuses a thesis-cited field with code-shared semantics.**
   `OceanicAge` is the `a_o` of CGF Table 1 / thesis Table 3.2. The same
   `ComputePlateOceanicAge` aggregate already drives IIIB's
   `OlderOceanicUnderYoungerOceanic` polarity decision class. Reusing it
   here means the new resolver inherits IIIB's existing
   `FMath::IsNearlyEqual(..., 1.0e-6)` tolerance discipline.
3. **Layer 3 is purely deterministic.** Plate IDs are unique integers;
   the comparison is total. This guarantees the rule terminates with a
   single winner for every same-distance multi-valid pair, regardless of
   field state.
4. **Forbidden policies remain forbidden.** None of the layers consult
   prior global sample owner, projection-derived authority, centroid /
   random / synthetic winners, or barycentric coordinates of the candidate
   triangles. The existing `bUsedPriorOwnerFallback` / `bUsedPolicyWinner`
   counters stay at zero on every resolved record; `bUsedSharedBoundaryTieBreak`
   is the new positive flag.
5. **It is honest about not being thesis-cited.** Direction at Layer 2
   (older-wins) is acknowledged as a weak axis. Layer 3 is acknowledged
   as setup-order arbitrary. The slice report and consolidation must
   frame the rule as approved lab policy.

## Implementation packet for Codex

### Files to touch

- `Source/CarrierLab/Private/CarrierLabVisualizationActor.cpp` — selector
  hook in the anonymous-namespace helper `SelectPhaseIIIE3FilteredRemeshSource`
  at line 2353-2462. The new resolver runs **between** the existing
  within-plate coalescing path (line 2437-2459) and the fail-loud
  `OutRecord.SelectionClass = ClassifyIIIE3UnresolvedMultiHit(...)`
  assignment at line 2460-2461. The IIIE.6.2 snapshot builders
  `BuildPhaseIIIE62CandidateSnapshot` (line 1978) and
  `DiagnosePhaseIIIE62HoldFromSnapshots` (line 2118) are the existing
  classifier; the new code calls them on the `VisibleCandidates` set to
  obtain the shape class, then dispatches.
- `Source/CarrierLab/Private/CarrierLabVisualizationActor.cpp` —
  `FCarrierLabPhaseIIIE3SelectionRecord` audit struct and
  `AccumulatePhaseIIIE3Record` (line 2465+) extended with the new fields
  and counters specified below. Aggregate wiring in
  `RunPhaseIIIE3FilteredRemeshSelectionAuditForSamples` (line 6392+).
- `Source/CarrierLab/Private/CarrierLabVisualizationActor.cpp` — live
  cadence consumer `ApplyPhaseIIIELiveRemeshEvent` (line 10936+) updates:
  the previously fail-loud branch at line 10985-10993 must now treat
  records resolved by the tie-break as `bResolvedSingleHit = true` for
  downstream IIIE.4/IIIE.5 routing. The `LastRemeshMode` string format
  should include `tie_break_resolved=N` summary fields. Records still in
  `UnresolvedMultiHitCount` (held classes only) must continue to fail
  loud.
- `Source/CarrierLab/Public/CarrierLabCarrier.h` — no schema change. All
  new fields live on the audit-record types in the cpp file.
- `Source/CarrierLab/Private/Commandlets/CarrierLabPhaseIIIE63MultiHitTieBreakCommandlet.cpp`
  — new commandlet, modeled on `CarrierLabPhaseIIIE6_2CrossPlateMultiHitCommandlet.cpp`.
  Owns the per-class fixture gates and the default-100k/40 distribution
  gate.
- `Source/CarrierLab/Private/Commandlets/CarrierLabPhaseIIIE6_2CrossPlateMultiHitCommandlet.cpp`
  — keep working as a baseline diagnostic. Add a fixture-level toggle
  `bDisableSharedBoundaryTieBreak = true` that the diagnostic threads
  through to the selector so the historical 5,845 distribution remains
  reproducible. (See "Preservation of IIIE.6.2 baseline" below.)
- `docs/checkpoints/phase-iii-slice-iiie6-3-multihit-tiebreak-report.md`
  — new slice report at completion (modeled on
  `phase-iii-slice-iiie6-2-cross-plate-multihit-diagnosis.md`).
- `docs/checkpoints/phase-iii-slice-iiie6-report.md` — append a note that
  the "unresolved multi-hit holds" Open Decision is now resolved by IIIE.6.3
  as a fifth named lab policy. Don't overwrite the existing report; just
  append a "Resolution" line.

### New audit fields (per-record on `FCarrierLabPhaseIIIE3SelectionRecord`)

```cpp
// Approved IIIE.6.3 lab-policy resolver, distinct from forbidden bUsedPolicyWinner.
bool bUsedSharedBoundaryTieBreak = false;

// Which IIIE.6.2 shape class the resolver acted on (only set when
// bUsedSharedBoundaryTieBreak is true). One of:
//   TwoPlateSharedGlobalEdge / TwoPlateSharedGlobalVertexOnly / ThreePlateCommonGlobalVertex.
ECarrierLabPhaseIIIE63SharedBoundaryShape SharedBoundaryShapeClass
    = ECarrierLabPhaseIIIE63SharedBoundaryShape::None;

// Which layer of the hierarchy produced the final winner.
//   ContinentalPriority / OlderOceanicAge / LowerPlateId / NotApplied.
ECarrierLabPhaseIIIE63TieBreakRule SharedBoundaryTieBreakRule
    = ECarrierLabPhaseIIIE63TieBreakRule::NotApplied;

// Per-candidate aggregates captured for audit. Indexed by candidate position
// in the visible-candidate list at hook time.
TArray<double> SharedBoundaryCandidateContinentalFractions;
TArray<double> SharedBoundaryCandidateOceanicAges;
TArray<int32>  SharedBoundaryCandidatePlateIds;
```

The existing forbidden-policy fields stay where they are and stay false on
tie-break-resolved records:

```cpp
bool bUsedPolicyWinner          = false; // IIIE.1 forbidden-policy flag, unchanged
bool bUsedPriorOwnerFallback    = false; // unchanged
// (projection-authority counter on the topology rebuild side, unchanged)
```

### New aggregate counters (on `FCarrierLabPhaseIIIE3RemeshSelectionAudit`)

```cpp
// Per-class resolved totals.
int32 SharedBoundaryEdgeResolvedCount       = 0;
int32 SharedBoundaryVertexOnlyResolvedCount = 0;
int32 SharedBoundaryThreePlateResolvedCount = 0;

// Per-rule fired totals (sum equals the per-class total).
int32 SharedBoundaryRuleContinentalPriorityCount = 0;
int32 SharedBoundaryRuleOlderOceanicAgeCount     = 0;
int32 SharedBoundaryRuleLowerPlateIdCount        = 0;

// Held-class totals (must stay >= 0 and the still-held shapes must be
// counted in UnresolvedMultiHitCount).
int32 SharedBoundaryHeldFieldMismatchCount         = 0;
int32 SharedBoundaryHeldNoSharedVerticesCount      = 0;
int32 SharedBoundaryHeldNonBoundaryOverlapCount    = 0;
int32 SharedBoundaryHeldThreePlateEdgeIntruderCount= 0;
int32 SharedBoundaryHeldThreePlateNoCommonCount    = 0;
int32 SharedBoundaryHeldInvalidUnclassifiedCount   = 0;

// Diagnostic baseline preservation flag.
bool  bSharedBoundaryTieBreakDisabled = false; // true only on the IIIE.6.2 baseline path
```

Invariant the audit must enforce at the end of
`RunPhaseIIIE3FilteredRemeshSelectionAuditForSamples`:

- When `bSharedBoundaryTieBreakDisabled == false`:
  - `SharedBoundaryEdgeResolvedCount + SharedBoundaryVertexOnlyResolvedCount +
    SharedBoundaryThreePlateResolvedCount` equals the sum of
    `SharedBoundaryRuleContinentalPriorityCount +
    SharedBoundaryRuleOlderOceanicAgeCount +
    SharedBoundaryRuleLowerPlateIdCount`.
  - `UnresolvedMultiHitCount` equals the sum of the six held-class counters.
  - `PolicyWinnerCount == 0` and `PriorOwnerFallbackCount == 0` (forbidden
    policies remain forbidden).
- When `bSharedBoundaryTieBreakDisabled == true` (IIIE.6.2 diagnostic
  baseline):
  - All `SharedBoundary*ResolvedCount` and `SharedBoundaryRule*Count` are
    zero.
  - `UnresolvedMultiHitCount` reproduces IIIE.6.2's measured 5,845.

### New gate fixtures (one IIIE.6.3 commandlet)

| Fixture | Class | Setup | Pass criterion |
|---|---|---|---|
| `two-plate shared global edge — continental beats oceanic` | `TwoPlateSharedGlobalEdge` | One continental plate, one oceanic plate sharing a global edge. | Resolved to continental plate; rule = `ContinentalPriority`; per-record `bUsedSharedBoundaryTieBreak = 1`; `bUsedPolicyWinner = 0`; `bUsedPriorOwnerFallback = 0`; record-hash matches expected. |
| `two-plate shared global edge — same status, older oceanic wins` | `TwoPlateSharedGlobalEdge` | Two oceanic plates sharing a global edge with distinct plate-level ages. | Resolved to older plate; rule = `OlderOceanicAge`; record-hash matches. |
| `two-plate shared global edge — same status, age tie, lower id wins` | `TwoPlateSharedGlobalEdge` | Two oceanic plates with equal plate-level age (within ε). | Resolved to lower-id plate; rule = `LowerPlateId`; record-hash matches. |
| `two-plate shared global vertex only — same hierarchy applies` | `TwoPlateSharedGlobalVertexOnly` | Vertex-only adjacency, continent vs ocean. | Resolved to continental plate; rule = `ContinentalPriority`. |
| `three-plate common global vertex — pairwise elimination` | `ThreePlateCommonGlobalVertex` | One continental, two oceanic with distinct ages. | Resolved to continental plate; rule = `ContinentalPriority`. |
| `three-plate common global vertex — three oceanic, two ages tie` | `ThreePlateCommonGlobalVertex` | All oceanic; two plates have age `A`, one has age `B > A`. | Resolved to plate at age `B`; rule = `OlderOceanicAge`. |
| `three-plate common global vertex — three oceanic, all ages tie` | `ThreePlateCommonGlobalVertex` | All oceanic, all ages equal within ε. | Resolved to lowest-id plate; rule = `LowerPlateId`. |
| `two-plate field mismatch — held` | `TwoPlateFieldMismatch` | Distinct fields on the two candidates. | Stays unresolved; `bUsedSharedBoundaryTieBreak = 0`; counted in `SharedBoundaryHeldFieldMismatchCount` and in `UnresolvedMultiHitCount`. |
| `two-plate no shared global vertices — held` | `TwoPlateNoSharedGlobalVertices` | Coincident hits with no shared global TDS feature. | Stays unresolved; counted in `SharedBoundaryHeldNoSharedVerticesCount`. |
| `non-boundary interior overlap — held` | `NonBoundaryOrInteriorOverlap` | Two candidates with interior barycentrics on different plates. | Stays unresolved; counted in `SharedBoundaryHeldNonBoundaryOverlapCount`. |
| `default 100k/40 replay` | mixed | Default actor config (100,000 samples, 40 plates, seed 42). | Same-seed determinism, distribution: edge `3740`, vertex-only `2021`, three-plate `84`, all six held-classes `0`; sum-resolved `5845`; `UnresolvedMultiHitCount = 0`; live-cadence remesh succeeds end-to-end. |
| `default 100k/40 IIIE.6.2 baseline replay` | mixed | Default actor config with `bDisableSharedBoundaryTieBreak = true`. | Reproduces IIIE.6.2's `selection 6da5cc74b3713bd4 / diagnosis ca9995af2500639c` hashes verbatim (or whatever the recompute yields after the per-record schema additions; see hash regression strategy). |
| `same-seed selection replay` | mixed | Run the full IIIE.6.3 audit twice. | Identical aggregate hash on the second run. |
| `forbidden-policy regression` | mixed | Default config. | `PolicyWinnerCount == 0`, `PriorOwnerFallbackCount == 0`, projection-authority counter `0` after live remesh. |

### Acceptance criteria (full set for the slice)

1. All seven new per-class fixtures pass with expected per-record hashes
   recorded in the slice report.
2. All four held-class fixtures pass with `bUsedSharedBoundaryTieBreak = 0`
   and the appropriate held counter incremented.
3. Default 100k/40 replay produces edge `3740`, vertex-only `2021`,
   three-plate `84`, held classes all `0`, sum `5845`, and
   `UnresolvedMultiHitCount = 0`. Distribution matches IIIE.6.2's per-class
   counts exactly.
4. Default 100k/40 IIIE.6.2 baseline replay (with tie-break disabled)
   reproduces the IIIE.6.2 selection and diagnosis hashes (allowing for
   schema-additive hash drift; see hash regression below).
5. Same-seed determinism: two runs of the IIIE.6.3 commandlet produce
   identical aggregate and per-record hashes.
6. The 10 existing IIIE gate suites (IIIE.1, IIIE.2, IIIE.3, IIIE.4, IIIE.5,
   IIIE.6, IIIE.6.1 same-plate coalescing, IIIE.6.2 diagnostic, plus the
   inherited IIIB6/IIID7 regression artifacts that IIIE consumers run) all
   still pass.
7. Live actor smoke at default config (100k/40 seed 42) completes one
   `ApplyPhaseIIIELiveRemeshEvent` end-to-end without
   `phase_iii_e6_live_hold_unresolved_multi_hit_*` mode strings on the
   resolved-classes path. Held-class fixtures still produce a
   fail-loud mode string when constructed deliberately.
8. `PolicyWinnerCount == 0` and `PriorOwnerFallbackCount == 0` on every
   pass.
9. `bUsedSharedBoundaryTieBreak`, the per-record shape enum, and the
   per-record rule enum are recorded for every resolved record and visible
   in the slice report's evidence rows.

### Disclosure language for the IIIE.6.3 slice report

(Models the IIIE.4 / IIIE.5 / IIIE.6 reports.)

> ## Approved Lab Policy
>
> The IIIE.3 selector is extended with a single approved CarrierLab lab
> policy for boundary-shared multi-valid same-distance ray hits:
> shared-boundary tie-break by hierarchical (continental priority → older
> plate-level oceanic age → lower plate id). The rule operates only on
> three IIIE.6.2 shape classes — `TwoPlateSharedGlobalEdge`,
> `TwoPlateSharedGlobalVertexOnly`, `ThreePlateCommonGlobalVertex` — which
> the architectural verification at
> `docs/checkpoints/phase-iii-architectural-deviation-verification.md`
> established are legitimate boundary ambiguities arising from the
> duplicated-per-plate-from-global-TDS architecture both the thesis
> §3.2.4 and CGF §6 prescribe. The thesis and CGF paper are silent on the
> tie-break rule; this is **not** a paper-cited rule and is not a claim
> of paper-faithfulness.
>
> Three-plate cases are resolved by single-elimination tournament under
> the same hierarchy, producing a single sample-level owner. Sample-level
> splitting is explicitly out of scope: thesis §3.3.2.3 step 3 (page 69)
> partitions the global TDS by **single per-vertex plate index**, and
> CarrierLab's `FCarrierLabPhaseIIIE5RemeshVertexRecord.AssignedPlateId`
> matches that contract.
>
> Records resolved by the rule carry `bUsedSharedBoundaryTieBreak = true`
> with `SharedBoundaryShapeClass` (which class) and
> `SharedBoundaryTieBreakRule` (which layer fired). The forbidden-policy
> counters `bUsedPolicyWinner`, `bUsedPriorOwnerFallback`, and the
> projection-authority counter remain zero on every resolved record.
> Records in held classes (`TwoPlateFieldMismatch`,
> `TwoPlateNoSharedGlobalVertices`, `ThreePlateEdgePlusIntruder`,
> `ThreePlateNoCommonSourceVertex`, `NonBoundaryOrInteriorOverlap`,
> `InvalidOrUnclassified`) continue to fail loud.

### Disclosure language for the IIIE consolidation

(Replaces the open-decision row left by IIIE.6.)

> ## Approved CarrierLab Lab Policies in IIIE
>
> IIIE consolidation acknowledges five named lab policies, each disclosed
> as approved CarrierLab lab extensions and **not** as paper-cited rules:
>
> 1. **Geophysics-derived sqrt-subsidence zGamma profile** (IIIE.4).
>    Anchored to thesis Table 3.2 ridge/abyssal constants but with no
>    closed-form profile law in the thesis or CGF. Records carry
>    `bUsedZGammaGeophysicsDerivedProfile = 1` and
>    `bPaperFaithfulZGammaProfile = 0`.
> 2. **Two-of-three majority assignment for mixed global-TDS triangles**
>    (IIIE.5). Mixed triangles whose three vertices include exactly two
>    sharing one assigned plate index are awarded to that plate.
> 3. **Triple-junction centroid-split topology for one-one-one mixed
>    global-TDS triangles** (IIIE.5). Subdivides the triangle into per-
>    plate boundary wedges; no whole-triangle winner.
> 4. **Continental overwrite → rifting-pending route** (IIIE.6).
>    Divergent oceanic generation over pre-existing continental material
>    is recorded as `rifting pending` and not applied to topology; IIIF
>    rifting will own the actual replacement.
> 5. **Shared-boundary multi-hit hierarchical tie-break** (IIIE.6.3).
>    Continental priority → older plate-level oceanic age → lower plate
>    id, applied to the IIIE.6.2 boundary-shared shape classes only.
>    Other unresolved-multi-hit shapes continue to fail loud.
>
> None of these policies is paper-cited. All are deterministic. All carry
> per-record audit flags distinguishing approved-policy use from the
> forbidden uncited-winner / prior-owner / projection-derived /
> centroid-random-synthetic policies, which remain forbidden and stay at
> zero counters.

### Hash regression strategy

The IIIE.6.2 diagnosis hashes (`selection 6da5cc74b3713bd4`,
`diagnosis ca9995af2500639c`) were recorded **before** the IIIE.6.3 audit
schema added per-record fields. Two cases:

1. **Schema-additive drift on the diagnostic baseline.** If IIIE.6.3 adds
   the new per-record fields into the IIIE.6.2 audit struct (e.g., for
   uniformity), the IIIE.6.2 baseline hashes will change even with
   `bDisableSharedBoundaryTieBreak = true`. Codex must recompute and
   record the new baseline hashes in the IIIE.6.3 slice report, framing
   them as derived from the schema addition (zero-valued tie-break fields
   hashed in), **not** from a behavior change. The slice report must
   include a paragraph naming both the old IIIE.6.2 hashes and the new
   schema-augmented hashes, with a one-line attribution.
2. **No drift if schema is kept disjoint.** Alternatively, the new fields
   live only on the IIIE.6.3 audit path; the IIIE.6.2 commandlet keeps
   its existing audit struct unchanged. Then the IIIE.6.2 hashes match
   verbatim. This is the cleaner choice and is preferred unless
   audit-path code-sharing requires the schema merge.

The IIIE.6.3 default 100k/40 hash is **new evidence**, derived from the
new tie-break behavior. It must be **computed and recorded** by Codex,
not pasted from prior buggy or pre-rule states. Same-seed determinism
re-run twice must produce the same hash.

The 10 existing IIIE gate hashes (IIIE.1 through IIIE.6 plus IIIB6/IIID7
inherited regressions) are upstream of the selector and should not change.
If any do, that is a regression and the slice fails its acceptance
criteria.

### Preservation of IIIE.6.2 baseline

IIIE.6.2 stays callable as a baseline diagnostic. The mechanism:

- Add a `bDisableSharedBoundaryTieBreak` flag on the existing IIIE.6.2
  commandlet's run options. Default false (tie-break enabled).
- The IIIE.6.2 commandlet's existing default-100k/40 fixture passes
  `bDisableSharedBoundaryTieBreak = true` so it reproduces the historical
  unresolved distribution `5845 = 3740 + 2021 + 84` plus the held classes
  at zero.
- The flag threads down to `SelectPhaseIIIE3FilteredRemeshSource` via the
  same plumbing the existing `bEnableDuplicateHitCoalescing` parameter
  uses (already present at line 2357). No new threading is needed beyond
  one bool.
- Selecting "tie-break disabled" forces every shape — including the
  three resolved classes — to fail loud as `bUnresolvedMultiHit = true`,
  exactly as IIIE.6.2 originally measured.

This preserves IIIE.6.2 as the diagnostic-only baseline the
checkpoint at
`docs/checkpoints/phase-iii-slice-iiie6-2-cross-plate-multihit-diagnosis.md`
already commits the project to maintaining.

### Default-OFF question

The standing user feedback memory says new feedback loops into authoritative
state should default OFF. IIIE.6.3 is **not a new feedback loop**; it
closes a hole in an existing remesh path that already feeds plate-local
state into selection (the existing single-hit case already does this).
The new resolver consumes plate-level aggregates (`ContinentalFraction`,
`OceanicAge`) that are already authoritative across IIIB; it does not
introduce a new authoritative-state-to-process feedback edge.

Default-ON is therefore the correct disposition for IIIE.6.3 in the live
cadence, with the IIIE.6.2 baseline diagnostic preserved as opt-in
disablement. If user disagrees and wants belt-and-suspenders default-OFF
with paper-faithful runs opting in, the slice can flip the default with
no other changes; this is a one-line decision Codex can take from the
slice prompt.

## Open questions

1. **Older-vs-younger direction at Layer 2.** The recommendation is
   older-wins for IIIB code-reuse symmetry. The geophysics underlying
   IIIB.5's `OlderOceanicUnderYoungerOceanicCount` is about which plate
   *subducts* (older subducts under younger because it is denser). That
   is not the same question as which plate *owns* a shared boundary
   sample. A defensible alternative is younger-wins (younger plates are
   the new lithosphere being created at the boundary). This is the
   single arguable axis in the recommendation. **User decision welcome.**
2. **ε for Layer-2 tolerance.** Recommendation reuses IIIB.5's
   `1.0e-6` Ma. If aggregate ages frequently land within `1.0e-6` of each
   other at production scale, Layer 3 would dominate; this is acceptable
   but should be observed in the default 100k/40 distribution. The rule
   counter breakdown in the slice report will show how often each layer
   fires.
3. **Plate area as a Layer-2 alternative.** Discarded in favor of older-
   oceanic-age because area is monotone-increasing under accretion and
   biases long-horizon runs toward supercontinent formation. If user
   prefers area, the substitution is one line and inherits the same
   tolerance discipline. Mentioning it here as a switchable axis.
4. **Hash regression schema choice.** The slice report must call out
   either "schema-disjoint, IIIE.6.2 baseline hashes preserved
   verbatim" or "schema-merged, IIIE.6.2 baseline hashes recomputed and
   noted". Codex picks; either is defensible. Disjoint is cleaner.
5. **Whether to add `bUsedSharedBoundaryTieBreak` to the
   `FConvergenceSubductionPolarityDecision` audit hash chain.** Probably
   not — the polarity audit is convergence-side, not selector-side, and
   conflating the two would muddle the audit lineages. Keeping the new
   counters local to `FCarrierLabPhaseIIIE3RemeshSelectionAudit` is the
   recommendation.

## Recommendation

The slice is ready for Codex execution as designed, **subject to one
optional user redirect on Open Question 1** (older-wins vs. younger-wins
at Layer 2). The hierarchy itself, its three-plate handling, the held
classes, the audit-by-construction discipline, and the disclosure language
are all defensible without further user decision. Codex can begin
implementation against this packet as-is, treating Open Question 1 as
"older-wins unless instructed otherwise" in the slice prompt. The
default-100k/40 distribution gate (5,845 → 0 unresolved with the measured
3,740 / 2,021 / 84 split) is the single most important acceptance signal;
if it does not reproduce, the rule has not actually been wired to the
classes the architectural verification said it should target.

## Summary for independent review

This design recommends that IIIE.6.3 land a single named lab policy — a
hierarchical multi-hit tie-break (plate-level continental priority → plate-
level older oceanic age → lower plate id) — that resolves the 5,845
boundary-shared multi-valid same-distance ray-cast holds the IIIE.3
selector currently fails loud on at the default 100k/40 seed-42
configuration. The design is grounded in three findings the planning agent
verified directly against page images: thesis §3.2.4 (`cc5c6807-066.png`,
p.55) confirms duplicate/re-index/re-compact plate-local triangulations from
a global TDS; thesis §3.3.2.3 step 2a (`cc5c6807-079.png`, p.68) describes
remesh ray-cast in singular language without a multi-valid tie-break; CGF
§6 (`aa42e52c-08.png`) and the thesis bibliography (CDS12 cited p.53,
Ren97 cited p.52) likewise provide no half-open or simulation-of-simplicity
convention to anchor a paper-cited rule. The recommended hierarchy uses
plate-level aggregates because IIIE.6.2 measured zero field-mismatch
holds, meaning candidate-interpolated fields tie at the boundary cases
the live cadence actually hits. Layer 1 reuses the IIIE.6 continental-
preservation discipline; Layer 2 reuses IIIB's existing
`ComputePlateOceanicAge` aggregate and 1.0e-6 Ma tolerance; Layer 3 is a
fully deterministic plate-id fallback. Three-plate cases resolve by
pairwise single-elimination under the same hierarchy, preserving the
single-plate-index-per-global-vertex contract of §3.3.2.3 step 3 and
keeping IIIE.5's centroid-split topology distinct from sample-level
splitting. The IIIE.6.2 diagnostic remains callable as a baseline via a
`bDisableSharedBoundaryTieBreak` opt-out so the original 5,845-hold
distribution stays reproducible. Forbidden-policy counters
(`bUsedPolicyWinner`, `bUsedPriorOwnerFallback`, projection authority)
remain zero; a new explicit `bUsedSharedBoundaryTieBreak` flag with shape-
class and rule-fired enums attests positively to the approved policy.
The IIIE consolidation will enumerate this as the fifth of five named
IIIE lab policies (sqrt zGamma, 2-of-3 majority, triple-junction
centroid-split, continental→rifting-pending, shared-boundary tie-break),
all approved as named lab extensions and none claimed as paper-cited.
