# Phase IIIE.6.5 Nearest-Valid-Hit Design

Audit target: head `6b10130` (`Diagnose IIIE6 live cadence multi-hit holds`).
IIIE.6.4 diagnostic landed at the same head and produced the JSONL evidence
this design depends on.

This is a planning-only checkpoint. It does not modify code. It specifies the
shape of an IIIE.6.5 implementation slice for Codex execution: a single named
lab policy that resolves the post-motion multi-hit holds the IIIE.3 selector
currently fails loud on at the default 100k/40 seed-42 configuration, after
IIIE.6.1 same-plate coalescing and IIIE.6.3 shared-boundary tie-break have
already run.

## Status

DESIGN READY. The slice resolves both `cross_plate_different` and
`third_plate` unique-nearest cases as a named CarrierLab lab policy, holds
distance ties fail-loud (no fallthrough to IIIE.6.3), and preserves manual
and automatic remesh path equivalence. IIIE.6.5 is a blocker-reduction
slice: at default 100k/40 it cuts post-motion holds from 24,600 to ~4,
but live remesh continues to hold (and does NOT mutate crust/projection
hashes) until the residual ~4 distance ties are eliminated by an
upstream geometry change or a future IIIE.6.6 policy. Two open questions
are flagged for optional user redirect (default-on vs default-off;
whether to surface a narrow IIIE.6.3 fallthrough at all). Otherwise
the slice is ready.

## Source 1 Result — IIIE.6.4 Re-Read

The IIIE.6.4 numbers are confirmed verbatim. Two clarifications and one
correction are necessary before the IIIE.6.5 design can use them.

### Confirmed numbers

Default 100k/40 seed 42, scenarios per `phase-iii-slice-iiie6-4-live-cadence-post-motion-multihit-diagnosis.md`:

| Scenario | Holds | crossDiff | third | process-marked | unique-nearest crossDiff | distance-tie crossDiff |
|---|---:|---:|---:|---:|---:|---:|
| manual_step_10 | 24277 | 22699 | 1578 | 0 | 22697 (99.99%) | 2 |
| manual_step_20 | 27038 | 22189 | 4849 | 0 | 22186 (99.99%) | 3 |
| manual_step_32 | 24600 | 16144 | 8456 | 0 | 16141 (99.98%) | 3 |
| manual_step_60 | 26451 | 16280 | 10171 | 0 | 16278 (99.99%) | 2 |
| auto_cadence_step_32 | 24600 | 16144 | 8456 | 0 | 16141 (99.98%) | 3 |

`manual_step_32` and `auto_cadence_step_32` converge on identical post-state
hashes `0be461d212b1a54a / ad21171e53f48d79 / 83de07ebd0edbf2a`. Process-marked
holds are zero across every scenario. The diagnostic mode string is
`phase_iii_e6_live_hold_unresolved_multi_hit_%d` and remesh events stay at
zero, so the live actor correctly does not claim success.

### Unit conversion

IIIE.6.4 reports gap distance in km, with median ~4e-6 km and p95 ~1.5e-5 km.
The unit conversion the user already corrected in the task brief is the right
one and bears repeating here:

- `1e-9 km` = `1 micrometer (1 µm)`.
- `4e-6 km` = `4 millimeters`. Not 4 micrometers.
- `1.5e-5 km` = `15 millimeters`.
- `2.5e-5 km` = `25 millimeters`.

The post-motion gap distribution is therefore "millimeter-to-centimeter scale
on planetary geometry," not micrometer-scale. The proposed `1e-9 km` tolerance
is approximately three to four orders of magnitude smaller than the median
observed gap. That margin matters for tolerance defensibility and is treated
in the Tolerance section below.

### Correction: third-plate unique-nearest evidence DOES exist

The IIIE.6.4 report's narrative says "Nearest-hit is computed as a parallel
diagnostic for `cross_plate_different` holds only," and its Nearest-Hit
Diagnostic table only surfaces cross-plate columns. This understates what
the slice actually recorded. Internally:

- The IIIE.6.4 audit struct already has
  `UniqueNearestThirdPlateCount` and `DistanceTieThirdPlateCount` fields
  (`CarrierLabVisualizationActor.cpp:2374-2384`); the accumulator increments
  them for `ECarrierLabPhaseIIIE3MultiHitBucket::ThirdPlate` records.
- The per-record JSONL `"type":"hold"` lines record `unique_nearest`,
  `nearest_distance_tie`, and `nearest_gap_km` for **every** held record,
  including third-plate ones. Per-record evidence at
  `Saved/CarrierLab/PhaseIII/IIIE64LiveCadencePostMotionMultiHit/phase-iii-slice-iiie6-4-live-cadence-post-motion-multihit-diagnosis-metrics.jsonl`.
- The per-scenario aggregate JSON line omits the third-plate fields, and
  the IIIE.6.4 markdown report's Nearest-Hit Diagnostic table mirrors that
  omission. That is a report-surface gap, not an evidence gap.

Counting third-plate hold records by `(scenario, unique_nearest, nearest_distance_tie)`
directly from the JSONL gives:

| Scenario | Third-plate total | Unique nearest | Distance tie | Other |
|---|---:|---:|---:|---:|
| manual_step_10 | 1578 | 1578 (100.00%) | 0 | 0 |
| manual_step_20 | 4849 | 4848 (99.98%) | 1 | 0 |
| manual_step_32 | 8456 | 8455 (99.99%) | 1 | 0 |
| manual_step_60 | 10171 | 10168 (99.97%) | 3 | 0 |
| auto_cadence_step_32 | 8456 | 8455 (99.99%) | 1 | 0 |

Sampling third-plate `nearest_gap_km` for `manual_step_32` (8455 records,
unique-nearest only): min `1.72e-9 km`, median `2.57e-6 km`, p95 `9.44e-6 km`,
max `2.47e-5 km`. The third-plate gap distribution is in the same
millimeter-scale order as the cross-plate distribution; nothing in the data
suggests third-plate cases need a separate tolerance or a different
classifier.

This evidence is decisive: third-plate is overwhelmingly unique-nearest, and
the residual distance ties are 0–3 per scenario, comparable to cross-plate.
**The IIIE.6.5 design therefore can resolve third-plate by the same
max-over-all-candidates unique-nearest rule as cross-plate** without an
audit-first deferral slice.

### Manual and automatic equivalence

Both manual and automatic remesh enter `SelectPhaseIIIE3FilteredRemeshSource`
through the same call site. The IIIE.6.4 evidence — identical post-state
hashes, identical hold counts, identical bucket distribution at step 32
between `manual_step_32` and `auto_cadence_step_32` — is the existing
guarantee. IIIE.6.5 must keep this invariant.

## Source 2 Result — Spatial Distribution

The PNG at `docs/checkpoints/phase-iii-slice-iiie6-4-spatial-distribution.png`
is five rows in scenario order (`manual_step_10`, `_20`, `_32`, `_60`,
`auto_cadence_step_32`), each with a left/right pair (Mollweide-style
projection of unit-sphere samples).

- Left panel — class color: red = `cross_plate_different`,
  magenta = `third_plate`.
- Right panel — diagnostic shape: cyan = unique nearest, blue = distance
  tie, yellow = process-marked.

Visual readout, treated as diagnostic only, not as visual approval:

1. **Process-marking is not the issue.** Yellow is not visible at all in
   any row. This matches the audit's `process-marked = 0` count and rules
   out the IIIB/IIIC/IIID upstream-marking redirect path the IIIE.6.4 stop
   conditions called out. There is no evidence pointing toward fixing
   upstream process-state filtering.
2. **Distribution is broad along plate boundaries.** Red and magenta swathes
   trace plate edges across all rows; the distribution is not localized to
   a single feature like a hot spot or a single triple junction. This is
   consistent with the IIIE.6.2 architectural finding that plate-local
   triangulations duplicate global-TDS boundary features and that motion
   creates ray-cast overlap along those duplicated boundaries.
3. **Third-plate (magenta) clusters near triple-junction-like
   configurations** and grows from row 1 to row 4 as motion accumulates
   (1578 → 4849 → 8456 → 10171 third-plate holds). This matches what one
   expects from three plates converging on shared neighborhoods after
   motion.
4. **Right panels are uniformly cyan.** Distance-tie blue is essentially
   invisible at PNG resolution; the visual confirms the JSONL counts (0–3
   ties per scenario, ~5 of 24600 holds at step 32). Unique-nearest is
   the dominant geometric resolution shape across all classes.
5. **No localized pathology.** The cyan / red overlap follows the global
   plate-boundary skeleton, not concentrated in a single basin or
   convergence anomaly. Nothing redirects from a nearest-hit policy
   toward an upstream geometric or topological correction.

Net: the spatial distribution supports post-motion ray-cast geometric
overlap as the underlying mechanism. It supports a nearest-hit lab policy
for the unique-nearest class. It does **not** suggest the holds are
artifacts of a separate upstream defect.

## Source 4 Result — Thesis Re-Read

Pages 65–68 (image filenames `cc5c6807-076.png` through `cc5c6807-079.png`)
re-read against the page images.

### §3.3.2 / §3.3.2.1 — Divergence (`cc5c6807-076.png`, p.65)

The text introduces divergence as the necessary counter-balance to
convergence and identifies its essential phenomena: oceanic-floor
generation along divergent ridges and plate fragmentation. Figure 39 shows
a triple junction between divergent plates with distance arrows
`d_i(p)` and `d_j(p)` to neighboring plates. This figure is the only
place in the divergence prose where pairwise distances to plates appear,
and the context is divergent-gap distance to the closest two plates,
**not** multi-valid hit disambiguation. There is no rule statement here
applicable to IIIE.6.5.

### §3.3.2.3 Implémentation, motivation paragraph (`cc5c6807-078.png`, p.67)

> « En effet, et pour la seule génération de plancher océanique, il
> faudrait être capable, à chaque pas de temps, d'identifier la création
> entre frontières, de redéfinir entre frontières, donc faire des
> insertions de nouveaux échantillons d'un coup et avant d'introduire de
> chevauchement entre plaques (qui seraient détectés depuis des zones
> de convergence sur l'algorithme) sans s'assurer de l'étanchéité de la
> planète, i.e. la couverture complète des zones de vide. Si on souhaite
> préserver l'interactivité de la simulation, calculer à chaque pas de
> temps l'augmentation des TDS local en garantissant ces contraintes est
> en fait une tâche difficile. »

This explicitly motivates the global TDS rebuild approach as a response
to the *fact* of plate overlap. The thesis acknowledges that overlapping
plates would "be detected from convergence zones by the algorithm." This
is the closest the thesis comes to recognizing the post-motion multi-valid
hit case. **It still does not name a winner rule for it.** The next
section is the alternative implementation, which the thesis describes
without revisiting the multi-valid hit semantics.

### §3.3.2.3 Échantillonnage step 2 (`cc5c6807-079.png`, p.68)

> « 2. Échantillonnage. Pour chaque sommet p de la TDS globale :
>
> a) On lance un rayon passant par le centre de la planète et p. Pour
> chaque plaque existante, nous testons l'intersection rayon-triangle
> via le BVH. Si l'intersection se fait avec **un triangle en
> subduction ou en collision**, nous ne la prenons pas en compte. Sinon
> et **s'il y a intersection**, nous procédons à l'**interpolation
> barycentrique** des paramètres de la croûte dans le triangle
> intersecté et nous les attribuons au sommet. Nous mémorisons pour ce
> sommet **l'indice de la plaque intersectée**. »

Three observations preserved from IIIE.6.3's re-read (verified again
against the page image):

1. The plate iteration is plural ("Pour chaque plaque existante"), but
   the consequence is singular ("le triangle intersecté", "la plaque
   intersectée"). The text presupposes uniqueness without asserting
   it. It does not say "the closest one wins"; it does not say "the
   first one wins"; it does not say "the highest-rank one wins." It
   simply assumes a single answer.
2. The only explicit per-plate filter is subduction/collision
   invisibility — exactly what IIIE.3's filter step already implements.
3. Step 2b (no-intersection divergent-gap branch) does use closest-frontier
   reasoning when partitioning the divergent gap to a plate index, e.g.
   "le point q1 le plus proche du sommet situé sur une frontière de
   plaque". This is the closest the thesis comes to "nearest" language
   for the assignment step. It is in the **no-hit** branch, not the
   multi-valid hit branch, and therefore is not a citation an IIIE.6.5
   nearest-hit rule can claim. It is, however, evidence that
   "closest" is part of the thesis's vocabulary in this exact assignment
   context, which is mildly relevant to engineering defensibility (see
   Recommended Rule below).

### Net result, IIIE.6.5

The thesis is silent on multi-valid post-motion ray-cast disambiguation.
The CGF 2019 paper (re-read against `aa42e52c-08.png` per IIIE.6.3 review)
is more compressed than the thesis and adds nothing not already in the
thesis prose. The IIIE.6.3 architectural verification correctly noted no
half-open or simulation-of-simplicity convention is inherited from the
cited literature. **IIIE.6.5 cannot be a paper-cited rule. It must be a
named CarrierLab lab policy.** The disclosure language must say so.

## Candidate Rule Analysis

Each candidate is weighed for: determinism, geometric/physical
defensibility, hidden-policy risk, auditability, and whether it scales
identically to manual and automatic remesh.

### Candidate A — Unique-nearest for `cross_plate_different` only

| Axis | Verdict |
|---|---|
| Determinism | Strong with strict `>`-tolerance comparison; ties remain ties. |
| Defensibility | High. Direct geometric measurement: ray distance to the actual hit point on the candidate triangle. |
| Hidden-policy risk | Low. Resolves only one bucket; third-plate stays held. |
| Auditability | High with positive `bUsedNearestHitTieBreak` flag, sub-enum, and per-record gap. |
| Manual/auto scaling | Yes. Same selector hook as today, no new branch difference. |
| Coverage of IIIE.6.4 holds | Resolves ~16,141 of 16,144 cross-plate-different cases at step 32 (99.98%); leaves ~8,456 third-plate cases held. Total holds reduce from 24,600 to ~8,459. |

Strength: keeps the slice narrow. Weakness: leaves a large third-plate
residual that IIIE.6.4 already shows is also overwhelmingly unique-nearest,
so this candidate underuses available evidence.

### Candidate B — Unique-nearest for both `cross_plate_different` and `third_plate`, max-over-all-candidates

| Axis | Verdict |
|---|---|
| Determinism | Same as A; max-over-all-candidates is associative and order-independent (whereas pairwise elimination is not). |
| Defensibility | High for both classes. The same ray-distance measurement is the most direct geometric quantity in both 2-plate and 3-plate cases. |
| Hidden-policy risk | Low if the third-plate evidence is published in the IIIE.6.5 report (otherwise risk that future readers think third-plate was extrapolated). |
| Auditability | High. Same fields as A plus a `UniqueNearestThirdPlate` sub-enum value. |
| Manual/auto scaling | Yes. |
| Coverage | Resolves ~16,141 cross-plate-different and ~8,455 third-plate at step 32 (24,596 of 24,600 holds, 99.98%). Distance ties (3 + 1 = 4) remain held. |

Strength: closes the bucket for both classes with the same rule and
matches the evidence. Weakness: requires the IIIE.6.5 report to publish
the third-plate breakdown that the IIIE.6.4 report omitted, so the
audit trail is fully visible.

### Candidate C — Unique-nearest with distance-tie fallthrough to IIIE.6.3

| Axis | Verdict |
|---|---|
| Determinism | Strong, but composes two rules across two distinct geometric classes. |
| Defensibility | Weak. The IIIE.6.4 distance-tie cases are not shared-boundary shapes; they are post-motion overlap with effectively-zero ray-distance separation. IIIE.6.3 was approved for shared-boundary same-distance cases, which is a different geometric class. |
| Hidden-policy risk | High. The IIIE.6.4 numbers show that for distance-tied cross-plate-different, "more-cont = 0, older = 0, lowerId = present". Falling through means continental priority and oceanic age both tie at 0, and Layer 3 (lower plate id) effectively resolves all distance ties. This silently expands lower-plate-id from a deterministic terminal fallback into a broad post-motion resolver. Forbidden by the IIIE.6.3 design's own intent. |
| Auditability | Possible with two visible flags (`bUsedNearestHitTieBreak = false`, `bUsedSharedBoundaryTieBreak = true`) but the audit honesty has to fight against the visible class merge. |
| Manual/auto scaling | Yes mechanically. |

Strength: would zero out the held count entirely. Weakness: silently
expands an existing rule beyond its sanctioned class. **Reject.**

### Candidate D — Lower-plate-id direct fallback for distance ties

A simpler form of C: skip the IIIE.6.3 invocation and just resolve
distance ties by lower plate id. Identical hidden-policy risk (lower
plate id becomes the post-motion resolver of last resort). **Reject.**

### Candidate E — Driftworld-style plate score / rank

Driftworld Tectonics (`Saved/ExternalRefs/driftworld-tectonics`)
verified at HEAD `b3e21c6...`. The relevant code:

- `Assets/Shaders/CSVertexDataInterpolation.compute`, kernel
  `CSCrustToData` lines 280–289: iterates all plates and updates
  `found_plate` only when `overlap_matrix[i * n_plates + found_plate] != -1`.
  This selects the plate that ranks higher under the overlap matrix.
- `Assets/Scripts/Planet.cs:937` `CalculatePlatesVP()`: builds plate
  scores as `score += -1 if oceanic, +1000 if continental` per crust
  vertex (line 948), ranks plates by score, then writes `m_PlatesOverlap`
  so the higher-rank plate "goes over" the lower-rank one.

Driftworld's policy is **plate-rank-by-continental-score**. It is **not**
nearest-distance. Driftworld pre-resolves the multi-overlap question via
an explicit overlap matrix before resampling, so the resampling kernel
itself never sees a nearest-distance choice — it sees only the
rank-decided "upper plate" at each contested point.

CarrierLab can defensibly diverge from Driftworld here. Two reasons:

1. **Different geometry of the multi-hit case.** Driftworld's overlap
   matrix is computed from a snapshot of plate-vertex distributions;
   it does not address post-motion ray-cast overlap as a separate
   per-sample question. CarrierLab's IIIE.3 selector is invoked
   per-sample at remesh time and observes per-sample ray distances.
   Nearest-distance is a more direct geometric measurement than
   continental-rank for the post-motion per-sample overlap case.
2. **IIIE.6.3 already absorbs Driftworld's continental-priority insight**
   for the same-distance shared-boundary case. Adding nearest-distance
   for the *different-distance* post-motion case complements rather
   than competes with the Driftworld-influenced rule.

Driftworld is therefore neutral evidence at best for IIIE.6.5: it
neither precludes nor recommends nearest-distance. The IIIE.6.5 disclosure
should note that Driftworld's overlap-matrix policy is structurally
different from CarrierLab's per-sample remesh selection and that
CarrierLab's choice of nearest-distance for post-motion overlap is a
divergence justified by the per-sample geometry, not a Driftworld-cited
rule.

### Candidate F — Continental priority extended to cross-plate-different

Re-applying IIIE.6.3 Layer 1 (higher plate-level continental fraction)
to cross-plate-different post-motion cases. IIIE.6.4 measured
`nearest_more_continental = 0` across every scenario. That means in the
post-motion holds, no candidate plate has a strictly larger plate-level
continental fraction than the others within tolerance — both candidates
are oceanic, or both are continental, at the resolution of plate-level
aggregates. **This rule resolves nothing**, regardless of correctness.

### Candidate G — Hold everything, treat IIIE.6.5 as diagnostic-only

The do-nothing baseline. Strength: zero rule risk. Weakness: leaves
~24,600 of 100,000 samples unable to remesh past step 32 in default
config, indefinitely. Live remesh remains effectively blocked. The user
already framed this slice as a **resolver** slice, so this is the
status quo we are choosing against.

### Per-class summary

| Class | IIIE.6.4 unique-nearest fraction | Recommended treatment |
|---|---|---|
| `cross_plate_different` | ~99.99% | Resolve by max-over-all-candidates unique-nearest. |
| `third_plate` | ~99.97–100.00% | Resolve by the same rule. |
| Distance ties (either bucket) | 0–3 per scenario, total ~4 of 100k | Hold fail-loud. No fallthrough. |
| `cross_plate_equal` (per existing IIIE.6.4 audit; 0 in default config) | n/a | Continue to fall through to existing classifier; not an IIIE.6.5 target. |
| Within-plate distance-separated, mixed-material, etc. | n/a | Continue to fall through to existing classifier; not an IIIE.6.5 target. |

## Recommended Rule

### Single rule, applied uniformly to two buckets

For each unresolved hold whose IIIE.6.4 multi-hit bucket is in
`{ ECarrierLabPhaseIIIE3MultiHitBucket::CrossPlateDifferent,
ECarrierLabPhaseIIIE3MultiHitBucket::ThirdPlate }`:

1. Build the candidate list at hook time. For each candidate, compute its
   ray-cast distance `d_i` to the candidate triangle (the same `Distance`
   already populated on `FCarrierLabIIIE3SelectionCandidate`).
2. Identify the candidate with minimum `d_i`. Let that distance be
   `d_min` and the next-smallest be `d_second`.
3. **Apply max-over-all-candidates unique-nearest test.** If
   `d_second - d_min > 1.0e-9 km` (strictly greater than tolerance), the
   minimum-distance candidate is the unique nearest and wins. Set
   `bUsedNearestHitTieBreak = true` and `NearestHitResultClass` to
   either `UniqueNearestCrossPlateDifferent` or `UniqueNearestThirdPlate`
   per the bucket.
4. **Otherwise hold.** If `d_second - d_min <= 1.0e-9 km`, the case is a
   distance tie. Set `bUsedNearestHitTieBreak = false`,
   `NearestHitResultClass = DistanceTieHeld`, and let the case fall to
   the existing fail-loud `ClassifyIIIE3UnresolvedMultiHit` path. The
   record remains counted in `UnresolvedMultiHitCount` exactly as before.
5. **Other buckets fall through unchanged.** If the bucket is anything
   other than `CrossPlateDifferent` or `ThirdPlate` (e.g.,
   `CrossPlateEqual`, `MixedMaterial`, `WithinPlateDistanceSeparated`),
   IIIE.6.5 does not fire. Set `NearestHitResultClass = UnsupportedHeld`
   so the audit explicitly records that IIIE.6.5 considered and declined
   the case. The case continues to fall through to the existing
   classifier.

### Three-plate handling

Max-over-all-candidates is the correct shape for the three-plate case.
For three candidates `{C0, C1, C2}` with distances `{d0, d1, d2}`, sort
ascending and apply the unique-nearest test on `min` vs `second-min`.
This is associative (same answer regardless of input order), unlike
pairwise single-elimination which can introduce order dependence under
very-close-distance fluctuations. The IIIE.6.4 audit code already uses
max-over-all-candidates semantics for `bHasUniqueNearest` (it tests
nearest vs second-nearest from the full candidate list) so IIIE.6.5
inherits the existing classification, no re-implementation needed.

### Distance-tie handling

**Hold fail-loud. No fallthrough to IIIE.6.3.** Reasoning:

- The post-motion distance-tie class is a different geometric class
  from IIIE.6.3's same-distance shared-boundary class. Falling through
  silently mixes two distinct geometric classes.
- Empirically (IIIE.6.4 numbers), distance-tied cross-plate-different
  cases have `nearest_more_continental = 0` and `nearest_older_oceanic = 0`,
  meaning IIIE.6.3 Layer 1 (continental priority) and Layer 2 (older
  oceanic age) would both tie. Layer 3 (lower plate id) would then
  resolve every distance tie. That is exactly the lower-plate-id-as-broad-
  resolver expansion the IIIE.6.3 design forbade.
- Count is small (0–3 per scenario, ~4 of 100,000 samples at default
  config). Holding them is acceptable; the live cadence will keep
  failing-loud on those specific samples and the report will say so.

If a future slice produces decisive evidence that the distance-tie
class is semantically equivalent to a shared-boundary class, an
IIIE.6.6 (or similar) can introduce a narrow, audited fallthrough.
Today, it is not justified.

### Tolerance: `1.0e-9 km` (1 micrometer)

Justification:

1. **Matches the IIIE.6.4 diagnostic threshold verbatim.** IIIE.6.4
   already classifies `bHasUniqueNearest` versus `bNearestDistanceTie`
   using `1e-9 km`. Reusing the same threshold means the IIIE.6.5
   resolver fires on exactly the cases IIIE.6.4 flagged as unique-nearest
   and holds exactly the cases IIIE.6.4 flagged as distance-tie. The
   audit story stays consistent across the two slices.
2. **Tolerance family precedent.** IIIE.6.3 uses `1.0e-9` for the
   continental-fraction dimensionless aggregate; IIIE.4 independent
   oracle uses tolerances in the `1e-6` family for scalar checks; IIIB.5
   uses `1.0e-6 Ma` for oceanic-age comparisons. `1.0e-9 km` for an
   absolute geometric distance is in the same precision family and is
   appropriate for a planetary-radius-scaled ray distance.
3. **Below observed gaps by orders of magnitude.** Median post-motion
   gap ~`4e-6 km`, p95 ~`1.5e-5 km`. Tolerance is roughly 4,000×
   smaller than the median. Min observed unique gap ~`1.72e-9 km` is
   just above tolerance — exactly at the edge where the tolerance
   correctly distinguishes unique-nearest from distance-tie.
4. **Equality is a hold.** `d_second - d_min == 1e-9 km` exactly is a
   hold (strict `>` test, not `>=`). Equality at or below tolerance
   never resolves; holds preserve fail-loud.

### Opt-out flag

Add `bDisableNearestHitTieBreak`, default false (rule active). When
true:

- The selector hook does not invoke the IIIE.6.5 resolver.
- All IIIE.6.5 audit fields revert to default values
  (`bUsedNearestHitTieBreak = false`, `NearestHitResultClass = NotApplied`,
  per-record gap = 0).
- All IIIE.6.5 aggregate counters are zero.
- Behavior is exactly equivalent to head `6b10130` for the same seed,
  to within the schema-additive hash drift documented below.

This mirrors `bDisableSharedBoundaryTieBreak` (IIIE.6.3) and gives the
IIIE.6.4 commandlet a clean way to reproduce the historical 24,600-hold
distribution as a diagnostic baseline.

### Manual/automatic equivalence

Both manual and automatic remesh enter
`SelectPhaseIIIE3FilteredRemeshSource`
(`CarrierLabVisualizationActor.cpp:2763`) through the same call site.
IIIE.6.5 inserts inside that helper, between the IIIE.6.3 shared-boundary
branch (line 2872–2895) and the unresolved-multi-hit classification
(line 2896–2916). No additional branch differences are introduced.

The IIIE.6.4 invariant `manual_step_32 == auto_cadence_step_32` post-state
hashes carries over to IIIE.6.5 by construction, because both paths call
the same helper with the same inputs. The slice gate must verify this
explicitly with a same-step manual-vs-automatic equivalence fixture.

## Implementation Packet For Codex

### Files to touch

- `Source/CarrierLab/Private/CarrierLabVisualizationActor.cpp`
  - Selector hook helper `SelectPhaseIIIE3FilteredRemeshSource` (line
    2763): insert the IIIE.6.5 resolver call between the existing
    IIIE.6.3 branch (line 2872–2895) and the unresolved-multi-hit
    classification (line 2896). New helper:
    `TryResolveIIIE65NearestHitTieBreak(VisibleCandidates, SampleId,
    bEnableNearestHitTieBreak, OutRecord, OutWinner)`.
  - `FCarrierLabPhaseIIIE3SelectionRecord` audit struct: add the
    per-record fields specified below.
  - `AccumulatePhaseIIIE3Record` (around line 2919–2990 region): wire
    the new aggregate counters.
  - `FCarrierLabPhaseIIIE3RemeshSelectionAudit` aggregate struct: add
    the new aggregate counters specified below.
  - Live cadence consumer
    `ApplyPhaseIIIELiveRemeshEvent` (around line 11790–11843): add
    `PhaseIIIELastNearestHitTieBreakCount` to `CurrentMetrics`; extend
    the held log line to include nearest-hit counters; preserve the
    `phase_iii_e6_live_hold_unresolved_multi_hit_%d` mode string for
    the residual distance-tie holds.
  - Public hash mixer (around line 3060): hash the new per-record
    fields.
- `Source/CarrierLab/Private/CarrierLabPhaseIIIE64LiveCadencePostMotionMultiHitCommandlet.cpp`
  - Pass `bDisableNearestHitTieBreak = true` in the IIIE.6.4 baseline
    fixtures so that commandlet keeps reproducing the historical
    24,600-hold distribution after IIIE.6.5 lands. Threading is one
    bool through `RunPhaseIIIE64PostMotionMultiHitDiagnosisAudit` to
    `SelectPhaseIIIE3FilteredRemeshSource`.
  - Extend the per-scenario aggregate JSON line and the report's
    nearest-hit table to surface the third-plate breakdown
    (`unique_nearest_third_plate`, `distance_tie_third_plate`) so the
    IIIE.6.4 evidence the IIIE.6.5 design depends on is visible in
    the original report after the schema lands. This is the
    "report-surface gap" called out in Source 1 above.
- `Source/CarrierLab/Private/CarrierLabPhaseIIIE65NearestHitCommandlet.cpp`
  - New commandlet, modeled on
    `CarrierLabPhaseIIIE63SharedBoundaryTieBreakCommandlet.cpp`. Owns
    the new fixtures and the default-100k/40 distribution gate.
- `Source/CarrierLab/Public/CarrierLabCarrier.h`
  - Likely no schema change. The new enums
    (`ECarrierLabPhaseIIIE65NearestHitResult`) and per-record fields
    live on the audit-record types in the cpp file unless the live
    metrics struct already in `.h` needs the new aggregate counter,
    in which case add only that one field.
- `docs/checkpoints/phase-iii-slice-iiie6-5-nearest-hit-report.md`
  - New slice report at completion. Must include the third-plate
    nearest-hit table verbatim (cross-plate AND third-plate) and the
    IIIE.6.4 baseline reproduction with `bDisableNearestHitTieBreak`.
- `docs/checkpoints/phase-iii-slice-iiie6-report.md`
  - Append a "Resolution: IIIE.6.5 nearest-hit lab policy" line under
    the existing open-decision row, mirroring how IIIE.6.3's resolution
    was appended.
- `docs/checkpoints/phase-iii-slice-iiie6-4-live-cadence-post-motion-multihit-diagnosis.md`
  - Optional addendum noting that the IIIE.6.4 nearest-hit diagnostic
    table omitted the third-plate breakdown (the JSONL evidence is
    present), and pointing at IIIE.6.5 as the slice that publishes
    the third-plate breakdown.

### Selector-hook integration order

The IIIE.6.5 resolver runs **between** existing IIIE.6.3 and the
fail-loud classifier:

```
Within SelectPhaseIIIE3FilteredRemeshSource:
  1. Filter visible candidates by FilterReason.
  2. If 0 raw candidates -> NoHitDivergentGap. Return.
  3. If 0 visible -> DivergentGapAfterFiltering. Return.
  4. If 1 visible -> ResolvedSingleHit. Return.
  5. Multi-hit:
     a. ClassifyIIIE3MultiHitBucket / IsIIIE3CoalescingFieldMismatch.
     b. IIIE.6.1 within-plate coalescing.
        If resolved -> ResolvedSingleHit. Return.
     c. IIIE.6.3 shared-boundary tie-break.
        If resolved -> ResolvedSingleHit. Return.
     d. IIIE.6.5 NEW. If bucket in
        { CrossPlateDifferent, ThirdPlate } AND unique-nearest test
        passes (d_second - d_min > 1e-9 km):
        -> ResolvedSingleHit, bUsedNearestHitTieBreak = true,
           NearestHitResultClass = UniqueNearestCrossPlateDifferent or
           UniqueNearestThirdPlate. Return.
     e. ClassifyIIIE3UnresolvedMultiHit. bUnresolvedMultiHit = true.
        NearestHitResultClass set per actual reason
        (DistanceTieHeld, UnsupportedHeld). Return.
```

Order rationale:

- IIIE.6.5 fires **after** IIIE.6.3 because IIIE.6.3 was approved for
  shared-boundary shapes regardless of distance. Putting IIIE.6.5
  first would alter IIIE.6.3 behavior on shared-boundary shapes that
  happen to have non-zero ray-distance gaps post-motion. IIIE.6.5
  after IIIE.6.3 is purely additive.
- IIIE.6.5 fires **before** the fail-loud classifier because the whole
  point of the slice is to resolve cases that would otherwise be
  classified as unresolved.
- In practice, IIIE.6.4 evidence shows post-motion holds are dominated
  by `cross_plate_different` and `third_plate` buckets (not
  shared-boundary shapes), so IIIE.6.5 fires on essentially all
  post-motion holds and IIIE.6.3 fires on essentially no post-motion
  holds. The ordering matters for correctness of edge cases, not for
  bulk distribution.

### New per-record audit fields

On `FCarrierLabPhaseIIIE3SelectionRecord`:

```cpp
// Approved IIIE.6.5 lab-policy resolver, distinct from forbidden bUsedPolicyWinner.
bool bUsedNearestHitTieBreak = false;

// Sub-enum recording IIIE.6.5's disposition of the case. One of:
//   NotApplied             -- IIIE.6.5 not invoked or not applicable bucket.
//   UniqueNearestCrossPlateDifferent
//   UniqueNearestThirdPlate
//   DistanceTieHeld        -- gap <= tolerance, held fail-loud.
//   UnsupportedHeld        -- bucket not in { CrossPlateDifferent, ThirdPlate }.
ECarrierLabPhaseIIIE65NearestHitResult NearestHitResultClass
    = ECarrierLabPhaseIIIE65NearestHitResult::NotApplied;

// Geometric evidence captured for audit (set whenever IIIE.6.5 was invoked,
// regardless of resolved/held disposition).
double NearestHitGapKm        = 0.0;  // d_second - d_min
double NearestHitToleranceKm  = 0.0;  // 1e-9 by default; recorded so an
                                      // out-of-band tolerance change is visible.
int32  NearestHitCandidateCount = 0;  // Visible candidate count at hook time.
int32  NearestHitDistinctPlateCount = 0;
int32  NearestHitProcessMarkedRefCount = 0; // Cross-reference count for
                                            // process-marked candidates among
                                            // the visible set; expected 0 per
                                            // IIIE.6.4 evidence.
```

The existing forbidden-policy fields stay where they are and stay false on
nearest-hit-resolved records:

```cpp
bool bUsedPolicyWinner          = false;  // unchanged, IIIE.1 forbidden flag
bool bUsedPriorOwnerFallback    = false;  // unchanged
// (projection-authority counter on the topology rebuild side, unchanged)
```

The IIIE.6.3 fields (`bUsedSharedBoundaryTieBreak` and friends) also stay
false on nearest-hit-resolved records, because the two resolvers handle
distinct geometric classes and never co-fire on the same record.

### New aggregate counters

On `FCarrierLabPhaseIIIE3RemeshSelectionAudit`:

```cpp
// Per-class resolved totals.
int32 NearestHitCrossPlateDifferentResolvedCount = 0;
int32 NearestHitThirdPlateResolvedCount          = 0;

// Held-class totals (must stay >= 0 and the still-held cases must be
// counted in UnresolvedMultiHitCount).
int32 NearestHitDistanceTieHeldCount             = 0;
int32 NearestHitUnsupportedHeldCount             = 0;

// Diagnostic baseline preservation flag.
bool  bNearestHitTieBreakDisabled = false; // true only on IIIE.6.4 baseline path.
```

Invariants the audit must enforce at the end of
`RunPhaseIIIE3FilteredRemeshSelectionAuditForSamples`:

- When `bNearestHitTieBreakDisabled == false`:
  - `NearestHitCrossPlateDifferentResolvedCount +
    NearestHitThirdPlateResolvedCount` equals the count of records with
    `bUsedNearestHitTieBreak == true`.
  - `NearestHitDistanceTieHeldCount + NearestHitUnsupportedHeldCount` is
    a subset (by class) of `UnresolvedMultiHitCount`. The other classes
    of unresolved holds (within-plate distance-separated, mixed-material,
    cross-plate-equal, etc.) continue to be classified by the existing
    classifier and are counted only in `UnresolvedMultiHitCount`.
  - `PolicyWinnerCount == 0` and `PriorOwnerFallbackCount == 0` (forbidden
    policies remain forbidden).
  - `SharedBoundaryTieBreakCount` continues to count exactly what
    IIIE.6.3 resolves; IIIE.6.5 does not increment it.
- When `bNearestHitTieBreakDisabled == true` (IIIE.6.4 diagnostic
  baseline):
  - All `NearestHit*ResolvedCount` are zero.
  - `UnresolvedMultiHitCount` reproduces IIIE.6.4's measured 24,600 at
    `manual_step_32` / `auto_cadence_step_32` (and the other scenarios'
    measured counts at their respective steps).

If both `bUsedNearestHitTieBreak` and `bUsedSharedBoundaryTieBreak` are
true on the same record, the audit must FATAL — that combination would
indicate a selector-hook ordering bug.

### New gate fixtures (IIIE.6.5 commandlet)

| Fixture | Bucket | Setup | Pass criterion |
|---|---|---|---|
| `cross-plate different — strict unique nearest resolves` | `CrossPlateDifferent` | Two-plate post-motion sample, distances differ by > tolerance. | Resolved to nearer plate; `bUsedNearestHitTieBreak = 1`; `NearestHitResultClass = UniqueNearestCrossPlateDifferent`; `NearestHitGapKm > 1e-9`; forbidden counters zero. |
| `third plate — strict unique nearest resolves` | `ThirdPlate` | Three-plate post-motion sample, distances all distinct. | Resolved to nearest plate; `NearestHitResultClass = UniqueNearestThirdPlate`. |
| `cross-plate different — distance tie at tolerance held` | `CrossPlateDifferent` | Two-plate sample with synthesized gap ≤ 1e-9 km. | Stays unresolved; `NearestHitResultClass = DistanceTieHeld`; counted in `NearestHitDistanceTieHeldCount` and in `UnresolvedMultiHitCount`. |
| `third plate — distance tie at tolerance held` | `ThirdPlate` | Three-plate sample with two candidates at gap ≤ 1e-9. | Stays unresolved; counted in held counters. |
| `cross-plate equal — IIIE.6.5 declines` | `CrossPlateEqual` | Pre-motion same-distance shared-boundary case (IIIE.6.3 territory). | IIIE.6.3 fires first and resolves; IIIE.6.5 records `NearestHitResultClass = NotApplied` (because IIIE.6.5 never sees the case). Audit: `bUsedSharedBoundaryTieBreak = 1`, `bUsedNearestHitTieBreak = 0`. |
| `mixed material — IIIE.6.5 declines` | `MixedMaterial` | Mixed-material multi-hit. | IIIE.6.5 records `NearestHitResultClass = UnsupportedHeld`; `bUsedNearestHitTieBreak = 0`; `UnresolvedMultiHitCount` increments via existing classifier. |
| `default 100k/40 seed-42 live selection (manual step 32)` | mixed | Default actor config, manual remesh at step 32. | Resolved ~16,141 `cross_plate_different` + ~8,455 `third_plate` = ~24,596 by IIIE.6.5; ~3 cross-plate + ~1 third-plate distance ties remain held; `UnresolvedMultiHitCount = 4`; live remesh stays held (events stay `0->0`, crust/projection hashes unchanged, `LastRemeshMode = phase_iii_e6_live_hold_unresolved_multi_hit_4`). |
| `default 100k/40 seed-42 live selection (auto cadence step 32)` | mixed | Default actor config, auto-cadence at step 32. | Identical resolved/held counts and identical held-state hashes / mode string to manual_step_32. |
| `default 100k/40 IIIE.6.4 baseline replay` | mixed | Default config with `bDisableNearestHitTieBreak = true`. | Reproduces IIIE.6.4's measured 24,600 holds at step 32 verbatim (allowing for schema-additive hash drift, see Hash Regression below). All `NearestHit*ResolvedCount` are zero; `bNearestHitTieBreakDisabled = true`. |
| `same-seed selection replay` | mixed | Run the IIIE.6.5 audit twice. | Identical aggregate hash on both runs. |
| `forbidden-policy regression` | mixed | Default config. | `PolicyWinnerCount == 0`, `PriorOwnerFallbackCount == 0`, projection-authority counter `0`. |
| `IIIE.6.3 + IIIE.6.5 do not co-fire` | mixed | Default config. | No record has both `bUsedSharedBoundaryTieBreak == 1` and `bUsedNearestHitTieBreak == 1`. |

### Acceptance criteria

1. Cross-plate-different unique-nearest fixture and third-plate
   unique-nearest fixture pass with the expected per-record evidence
   (resolved plate id, gap > tolerance, sub-enum value).
2. Distance-tie fixtures (cross-plate AND third-plate) stay held with
   `NearestHitResultClass = DistanceTieHeld` and increment
   `NearestHitDistanceTieHeldCount`.
3. Default 100k/40 seed-42 manual-step-32 fixture resolves
   `cross_plate_different` unique-nearest cases (~16,141) and
   `third_plate` unique-nearest cases (~8,455), holds the small
   distance-tie residual (~3 + ~1 = ~4), and produces identical
   post-state hashes between manual and automatic cadence at step 32.
4. Default 100k/40 IIIE.6.4 baseline replay (`bDisableNearestHitTieBreak = true`)
   reproduces IIIE.6.4's 24,600-hold distribution verbatim per scenario.
5. Same-seed determinism: two runs of the IIIE.6.5 commandlet produce
   identical aggregate and per-record hashes.
6. The 11 existing IIIE gate suites (IIIE.1 through IIIE.6 plus IIIE.6.1,
   IIIE.6.2, IIIE.6.3, IIIE.6.4, plus the inherited IIIB6/IIID7
   regression artifacts that IIIE consumers run) all still pass. None
   of them should observe IIIE.6.5 resolutions in their evidence
   tables, because IIIE.6.5 fires only on multi-hit holds the existing
   suites do not contain.
7. Live actor smoke at default config (100k/40 seed 42) calls
   `ApplyPhaseIIIELiveRemeshEvent` end-to-end:
   - The audit reports ~24,596 of 24,600 prior holds resolved by
     IIIE.6.5 (~16,141 cross-plate + ~8,455 third-plate) and ~4
     distance-tie holds remaining.
   - **Because residual unresolved holds exist, the event MUST stay
     held.** Events count stays `0 -> 0`, crust hash and projection
     hash do NOT change, and `LastRemeshMode` remains a fail-loud
     variant (e.g., `phase_iii_e6_live_hold_unresolved_multi_hit_4`).
   - This matches acceptance criterion 11 below; both criteria must
     agree. IIIE.6.5 is a large blocker-reduction slice (24,600 → ~4)
     but is **not** a live-visual unblock by itself; live remesh
     still holds while any unresolved tie remains. Final live unblock
     would require an IIIE.6.6 (or later) slice that names a policy
     for the residual distance-tie class — out of scope for IIIE.6.5.
8. `PolicyWinnerCount == 0`, `PriorOwnerFallbackCount == 0`, and the
   projection-authority counter `== 0` on every pass.
9. `bUsedNearestHitTieBreak`, `NearestHitResultClass`, and `NearestHitGapKm`
   are recorded for every record IIIE.6.5 considered, and visible in
   the slice report's evidence rows.
10. No record co-fires `bUsedSharedBoundaryTieBreak` and
    `bUsedNearestHitTieBreak`. The two resolvers must be mutually
    exclusive on a per-record basis.
11. **Honest live remesh acceptance:** the live-cadence consumer
    (`ApplyPhaseIIIELiveRemeshEvent`) must use `UnresolvedMultiHitCount`
    after IIIE.6.5 as the event-success gate. If
    `UnresolvedMultiHitCount > 0` (any residual distance ties or
    other held classes), the event count must NOT advance, the crust
    hash and projection hash must NOT change, and `LastRemeshMode`
    must remain a fail-loud variant (e.g.,
    `phase_iii_e6_live_hold_unresolved_multi_hit_4`). Only when
    `UnresolvedMultiHitCount == 0` may the event advance and the
    hashes change. There is no "succeeded with N ties held"
    intermediate state — events advance only when every visible
    multi-hit was resolved by an approved rule. The default 100k/40
    case has ~4 residual ties, so live remesh continues to hold;
    IIIE.6.5 is a blocker-reduction slice, not a live-visual unblock.

These criteria are deliberately weaker than "zero unresolved holds at
default config." Per the user's task brief: only resolve the classes
the design actually justifies, and report the residual honestly. ~4 of
100,000 unresolved samples is the intended outcome at default config,
and live remesh stays held until either IIIE.6.6 names a policy for
the distance-tie class or upstream geometry eliminates it.

### Hash regression strategy

Two cases:

1. **Schema-additive drift on the IIIE.6.4 baseline.** If IIIE.6.5 adds
   new per-record fields onto `FCarrierLabPhaseIIIE3SelectionRecord`
   (which it does), the per-record hash mixer at line ~3060 sees more
   bytes. Even with `bDisableNearestHitTieBreak = true`, the IIIE.6.4
   baseline hashes will change. Codex must recompute and record the new
   baseline hashes in the IIIE.6.5 slice report and in the IIIE.6.4
   commandlet's expected-hash table, framing them as derived from the
   schema addition (zero-valued IIIE.6.5 fields hashed in), **not**
   from a behavior change. The slice report must include both the old
   and the new IIIE.6.4 hashes with a one-line attribution to the
   schema addition.
2. **No drift on the upstream IIIE gates.** IIIE.1 through IIIE.5 do
   not touch the IIIE.3 selector record, so their hashes should not
   change. If any do, that is a regression and the slice fails its
   acceptance criteria.

The IIIE.6.5 default 100k/40 hash is **new evidence**, derived from the
new resolver behavior. It must be **computed and recorded** by Codex,
not pasted from prior states. Same-seed determinism re-run twice must
produce the same hash.

### Default-on vs default-off

Per user feedback memory ("New feedback loops default off"), sub-phases
introducing new feedback into authoritative state must default OFF.
IIIE.6.5 is **not a new feedback loop** in the same sense as IIIE.6
rifting-pending was: it does not introduce a new authoritative-state-to-
process feedback edge. It closes a hole in the existing IIIE.3 selector
which already feeds plate-local state into selection (the existing
single-hit case already does this; IIIE.6.1 and IIIE.6.3 do too).

By the same reasoning IIIE.6.3 used to land default-on, IIIE.6.5
should default-on. The IIIE.6.4 baseline diagnostic is preserved as
opt-in disablement via `bDisableNearestHitTieBreak`.

If user disagrees and prefers belt-and-suspenders default-off (e.g.,
"every newly-named lab policy stays off until I flip a switch"), the
slice can be implemented default-off with the same one-bool decision
applied at the call sites. This is Open Question 1 below.

## Disclosure Language

### IIIE.6.5 slice report

> ## Approved Lab Policy
>
> The IIIE.3 selector is extended with a single approved CarrierLab
> lab policy for post-motion multi-valid different-distance ray hits:
> nearest-valid-hit tie-break by max-over-all-candidates strict
> unique-nearest. The rule operates on two IIIE.6.4 multi-hit buckets
> — `CrossPlateDifferent` and `ThirdPlate` — which the IIIE.6.4
> diagnosis at
> `docs/checkpoints/phase-iii-slice-iiie6-4-live-cadence-post-motion-multihit-diagnosis.md`
> established are post-motion ray-cast overlap classes arising from
> plate motion against the duplicated-per-plate-from-global-TDS
> architecture both the thesis §3.2.4 and CGF §6 prescribe. Distance
> ties (gap ≤ 1e-9 km) remain fail-loud and are not resolved by this
> rule, by IIIE.6.3, or by lower-plate-id fallback.
>
> The thesis §3.3.2.3 step 2 ray-cast prose (`cc5c6807-079.png`, p.68)
> presupposes uniqueness of the per-sample plate intersection
> ("le triangle intersecté", "la plaque intersectée") without naming
> a winner rule for the multi-valid hit case. The CGF 2019 paper §6
> is more compressed than the thesis and adds nothing not already in
> the thesis prose. **This is not a paper-cited rule and is not a
> claim of paper-faithfulness.**
>
> Driftworld Tectonics, used as secondary implementation precedent in
> IIIE.6.3, structurally differs from CarrierLab here: Driftworld
> resolves multi-overlap via a precomputed continental-priority plate
> rank (`Assets/Scripts/Planet.cs:937` `CalculatePlatesVP()`,
> `Assets/Shaders/CSVertexDataInterpolation.compute:280-289`
> `CSCrustToData`), not by per-sample ray distance at remesh time.
> CarrierLab's choice of nearest-distance for post-motion overlap is
> a divergence justified by the per-sample geometry of the IIIE.3
> remesh path; it is not a Driftworld-cited rule.
>
> Three-plate cases are resolved by max-over-all-candidates
> unique-nearest, not by pairwise single-elimination. Max-over-all is
> associative and order-independent, while pairwise elimination can
> introduce ordering artifacts at sub-tolerance distance differences.
> Sample-level splitting remains explicitly out of scope: §3.3.2.3
> step 3 partitions the global TDS by single per-vertex plate index,
> matching CarrierLab's
> `FCarrierLabPhaseIIIE5RemeshVertexRecord.AssignedPlateId` contract.
>
> Records resolved by the rule carry `bUsedNearestHitTieBreak = true`
> with `NearestHitResultClass` (which sub-class), `NearestHitGapKm`
> (the strict unique-nearest gap), and per-record candidate evidence.
> Records held by the rule (distance-tie or unsupported bucket)
> carry `bUsedNearestHitTieBreak = false` and the appropriate
> sub-enum class, and continue to be counted in
> `UnresolvedMultiHitCount`. The forbidden-policy counters
> `bUsedPolicyWinner`, `bUsedPriorOwnerFallback`, the
> projection-authority counter, and the IIIE.6.3
> `bUsedSharedBoundaryTieBreak` counter remain zero on every
> nearest-hit-resolved record.

### IIIE consolidation (replaces IIIE.6 open-decision row)

> ## Approved CarrierLab Lab Policies in IIIE
>
> IIIE consolidation acknowledges six named lab policies, each
> disclosed as approved CarrierLab lab extensions and **not** as
> paper-cited rules:
>
> 1. **Geophysics-derived sqrt-subsidence zGamma profile** (IIIE.4).
> 2. **Two-of-three majority assignment for mixed global-TDS triangles**
>    (IIIE.5).
> 3. **Triple-junction centroid-split topology for one-one-one mixed
>    global-TDS triangles** (IIIE.5).
> 4. **Continental overwrite → rifting-pending route** (IIIE.6).
> 5. **Shared-boundary multi-hit hierarchical tie-break** (IIIE.6.3).
>    Continental priority → older plate-level oceanic age → lower plate
>    id, applied to IIIE.6.2 boundary-shared shape classes only.
> 6. **Post-motion nearest-valid-hit tie-break** (IIIE.6.5).
>    Max-over-all-candidates strict unique-nearest by ray distance,
>    applied to IIIE.6.4 `CrossPlateDifferent` and `ThirdPlate`
>    multi-hit buckets only. Distance ties remain fail-loud; no
>    fallthrough to other resolvers.
>
> None of these policies is paper-cited. All are deterministic. All
> carry per-record audit flags distinguishing approved-policy use from
> the forbidden uncited-winner / prior-owner / projection-derived /
> centroid-random-synthetic policies, which remain forbidden and stay
> at zero counters.
>
> The two tie-break policies (IIIE.6.3 and IIIE.6.5) handle
> structurally distinct geometric classes — same-distance
> shared-boundary versus different-distance post-motion overlap —
> and are mutually exclusive on a per-record basis: no IIIE.3 selection
> record co-fires both flags.

## Open Questions

1. **Default-on vs default-off.** The recommendation is default-on,
   matching the IIIE.6.3 disposition and reusing the same
   non-feedback-loop reasoning. If user prefers default-off ("new lab
   policies stay off until explicitly flipped"), the slice can be
   implemented default-off with an `Enable*` flag instead of a
   `Disable*` flag. **User decision welcome.**
2. **Whether to surface a narrow IIIE.6.3 fallthrough at all.** The
   recommendation is no fallthrough: distance ties hold fail-loud.
   If user disagrees, the alternative is a per-record fallthrough
   with both audit flags visible
   (`NearestHitResultClass = FallthroughSharedBoundaryTieBreak`,
   `bUsedSharedBoundaryTieBreak = 1`). The IIIE.6.4 evidence shows
   such a fallthrough would resolve essentially every distance-tie
   case by IIIE.6.3 Layer 3 (lower plate id), which the IIIE.6.3
   design itself characterized as a deterministic terminal fallback,
   not a broad resolver. The recommendation is to keep that boundary.
3. **Live remesh disposition while ~4 ties remain.** Resolved per
   acceptance criterion 11: the live event stays held while
   `UnresolvedMultiHitCount > 0`. No "succeeded with N held"
   intermediate state. The mode string keeps the existing fail-loud
   format (e.g., `phase_iii_e6_live_hold_unresolved_multi_hit_4`).
   This is the only safe disposition — anything else would mutate
   crust/projection hashes while the IIIE.3 selector still has
   visible unresolved multi-hits, which is the partial-remesh /
   hidden-fallback territory the IIIE.6 architecture explicitly
   forbids. Final live-visual unblocking requires a separate slice
   (IIIE.6.6 or later) that names a policy for the distance-tie
   class, or upstream geometry that eliminates the ties; the
   IIIE.6.5 slice does not make that decision.
4. **IIIE.6.4 report addendum.** The IIIE.6.4 nearest-hit table
   omitted the third-plate breakdown, although the JSONL evidence is
   present. Should the IIIE.6.4 commandlet be patched to surface
   third-plate counts in its report, or is appending an addendum row
   to the IIIE.6.4 markdown report sufficient? The recommendation
   is patch the commandlet so future runs of IIIE.6.4 (with or
   without the IIIE.6.5 baseline-disable flag) carry both
   breakdowns. The slice report adds an explicit row; the IIIE.6.4
   commandlet code change is one line per scenario in the JSON
   format string and one column in the report-table.
5. **Schema-additive hash drift on IIIE.6.4 baseline.** Confirmed
   above; the slice report must publish both old and new IIIE.6.4
   baseline hashes with a clear "schema addition, zero-valued IIIE.6.5
   fields hashed in" attribution. Codex picks; either is defensible
   but publishing both is required.

## Recommendation

The slice is ready for Codex execution as designed, **subject to one
optional user redirect on Open Question 1** (default-on vs default-off).
The other open questions are scope-internal decisions Codex can take
from the slice prompt (recommendations: keep distance-tie hold; live
remesh stays held while any tie remains and does NOT mutate
crust/projection hashes; patch the IIIE.6.4 commandlet to surface
third-plate breakdown; publish both old and new IIIE.6.4 baseline
hashes). The default-100k/40 distribution gates (24,600 → ~4
unresolved at step 32, with ~16,141 cross-plate + ~8,455 third-plate
resolved) are the most important acceptance signals; if they do not
reproduce, the rule has not actually been wired to the buckets the
IIIE.6.4 evidence said it should target. The manual-vs-automatic
equivalence at step 32 is the second-most-important signal. IIIE.6.5
is explicitly a blocker-reduction slice: it cuts post-motion holds by
~99.98% but does **not** unblock live visual remesh in default config,
because the residual ~4 distance ties keep the live event held by
acceptance criterion 11. Final live-visual unblocking is a separate
slice (IIIE.6.6 or later) and is out of scope here.

## Summary For Independent Review

This design recommends that IIIE.6.5 land a single named lab policy —
post-motion nearest-valid-hit tie-break by max-over-all-candidates
strict unique-nearest at `1.0e-9 km` tolerance — that resolves the
post-motion `cross_plate_different` and `third_plate` multi-hit holds
the IIIE.3 selector currently fails loud on at the default 100k/40
seed-42 configuration, after IIIE.6.1 and IIIE.6.3 have already run.
The design is grounded in three findings the planning agent verified
directly: (a) IIIE.6.4 numbers re-read from
`docs/checkpoints/phase-iii-slice-iiie6-4-live-cadence-post-motion-multihit-diagnosis.md`
and from
`Saved/CarrierLab/PhaseIII/IIIE64LiveCadencePostMotionMultiHit/phase-iii-slice-iiie6-4-live-cadence-post-motion-multihit-diagnosis-metrics.jsonl`,
including a third-plate unique-nearest distribution counted directly
from the JSONL (1578/1578, 4848/4849, 8455/8456, 10168/10171,
8455/8456 unique-nearest per scenario, with 0–3 distance ties each)
that the IIIE.6.4 markdown report omitted from its surface table; (b)
the spatial-distribution PNG at
`docs/checkpoints/phase-iii-slice-iiie6-4-spatial-distribution.png`
showing broad plate-boundary distribution, no process-marking yellow,
and uniform unique-nearest cyan, consistent with post-motion ray-cast
overlap rather than upstream marking failure; (c) thesis re-read of
`cc5c6807-076.png` through `cc5c6807-079.png` (pages 65–68) confirming
the thesis presupposes uniqueness of step-2a ray-cast intersection
without naming a multi-valid winner rule, and CGF §6 adds nothing
beyond that. The recommended rule resolves only the two buckets where
IIIE.6.4 evidence is decisive (`CrossPlateDifferent` and `ThirdPlate`),
holds distance ties fail-loud (no fallthrough to IIIE.6.3, because the
post-motion distance-tie class is not the same geometric class as
IIIE.6.3's same-distance shared-boundary class, and falling through
would silently expand lower-plate-id into a broad post-motion resolver),
and inserts in the selector hook between IIIE.6.3 and the fail-loud
classifier so it is purely additive to existing IIIE.6.1/IIIE.6.3
behavior. The IIIE.6.4 diagnostic baseline remains reproducible via
`bDisableNearestHitTieBreak`, mirroring IIIE.6.3's
`bDisableSharedBoundaryTieBreak`. Forbidden-policy counters
(`bUsedPolicyWinner`, `bUsedPriorOwnerFallback`, projection authority,
prior owner fallback) remain zero; a new explicit
`bUsedNearestHitTieBreak` flag with a five-value sub-enum
(`NotApplied / UniqueNearestCrossPlateDifferent /
UniqueNearestThirdPlate / DistanceTieHeld / UnsupportedHeld`) attests
positively to the approved policy. The IIIE consolidation will
enumerate this as the sixth of six named IIIE lab policies (sqrt
zGamma, 2-of-3 majority, triple-junction centroid-split, continental→
rifting-pending, shared-boundary tie-break, post-motion nearest-hit),
all approved as named lab extensions and none claimed as paper-cited.
The IIIE.6.3 and IIIE.6.5 resolvers are mutually exclusive per record
and handle structurally distinct geometric classes; the audit must
fatal if both flags fire on the same record. IIIE.6.5 is explicitly
framed as a blocker-reduction slice and not a live-visual unblock: at
default 100k/40 seed-42 it leaves ~4 of 100,000 samples held as
distance ties, so live remesh still holds (events stay 0, crust and
projection hashes unchanged, fail-loud mode string preserved) per
acceptance criterion 11. Final live-visual unblocking is a separate
slice that requires either a named policy for the residual distance-tie
class (IIIE.6.6 or later) or upstream geometry that eliminates the
ties; IIIE.6.5 does not pretend to make that decision.
