# Phase IIIE Entry — IIID Collision Thesis-Faithfulness Audit

Status: planning artifact. Audit produced before IIIE entry to convert the medium-confidence verdict on IIID's thesis alignment into either high confidence or specific blocking findings. No code changes proposed; all entries are read-only verdicts.

Authority discipline applied here follows the lesson recorded after the 2026-05-03 slab-pull P1-3 episode: extraction docs (`paper-resampling-extraction.md`, `phase-iii-paper-process-extraction.md`) and adversarial Pro audits are flag-generators, not authoritative on thesis-faithfulness. **Direct page-image inspection is the only authority.** This audit cites Cortial PhD thesis (INSA Lyon, 2020) page images by their image filename and printed page number; image-to-page offset is `image - 11 = page` for numbered pages.

## Current Disposition

This checkpoint is accepted into the trail as a thesis-faithfulness audit artifact, not as evidence that the listed divergences have been fixed. Pre-IIIE.5 through Pre-IIIE.7 were used for performance containment after this audit was drafted, so references below to a "Pre-IIIE.5 thesis-faithfulness cleanup tranche" are superseded by this disposition. Findings 9, 20, and 33 remain open thesis-faithfulness follow-ups to triage before IIIE implementation work that depends on those surfaces or before IIIH long-horizon validation, whichever comes first. IIIE.1 remains a no-code remesh-contract audit and may use this document as input.

## Sources Used

- Thesis §3.3.1 Convergence (printed pages 56–64, image filenames `cc5c6807-067.png` through `cc5c6807-075.png`).
- Thesis §3.3.2 Divergence (printed pages 65–68, images `cc5c6807-076.png` through `cc5c6807-079.png`) — for IIID→IIIE handshake context.
- Implementation: `Source/CarrierLab/Private/CarrierLabVisualizationActor.cpp` and the IIID per-slice commandlets `CarrierLabPhaseIIID1Commandlet.cpp` through `CarrierLabPhaseIIID7Commandlet.cpp`.
- Existing checkpoints: `phase-iii-slice-iiid1-report.md` through `phase-iii-slice-iiid7-report.md`, `phase-iii-iiid-consolidated.md`, `phase-iii-pre-iiid3-audit-triage.md`.

## Verdict Categories

- **MATCH** — implementation matches thesis at face value, with traceable formula correspondence.
- **MATCH (lab-named)** — implementation makes a defensible interpretive choice; thesis under-specifies; lab named the decision in its own docs.
- **AMBIGUOUS** — thesis is internally ambiguous; implementation picked one valid reading. Should be labeled as a lab interpretive choice rather than claimed as paper-faithful.
- **DIVERGE** — implementation differs from thesis in a way that is neither under-specified nor ambiguous.
- **GAP** — thesis specifies an algorithm; implementation doesn't yet have it.
- **THESIS-LIMIT** — thesis itself acknowledges the case is unhandled; our implementation shares the limitation rather than fixing what the paper left open.
- **DEFERRED-TO-IIIE** — cannot be fully evaluated until IIIE lands the §3.3.2.3 remesh path.

## Audit Table

### 1. Subduction triggering rules (§3.3.1.1)

| # | Decision | Thesis source | Implementation | Verdict |
|---|---|---|---|---|
| 1 | Ocean-ocean polarity by age (older subducts) | thesis p. 57 (img 068): "toujours la subduction de la plaque la plus âgée" | IIIB.5 ocean-ocean age polarity | MATCH |
| 2 | Ocean-continental: oceanic always under continental | thesis p. 57 (img 068): "Une plaque océanique amorce toujours une subduction si elle est en face d'une plaque continentale" | IIIB.4 mixed-material polarity | MATCH |
| 3 | Continental-continental → obduction (always), then collision iff opposing mass significant | thesis p. 57 (img 068): "nous permettons l'entame d'une obduction dans le cas continental-continental, quelque soit la taille des terranes... cette obduction deviendra collision si et seulement si la masse continentale adverse est significative" | IIIB.4 emits `CollisionCandidate` for cont-cont; IIID.3 mass test gates collision acceptance | MATCH |

### 2. Subduction continuous uplift (§3.3.1.1, applied per timestep)

| # | Decision | Thesis source | Implementation | Verdict |
|---|---|---|---|---|
| 4 | Uplift formula `ũ_j(p) = u_0 · f∘d(p) · g∘v(p) · h∘z̃_i(p)` | thesis p. 58 (img 069) | IIIC.3 oracle gate residual at FP epsilon | MATCH |
| 5 | Distance transfer `f∘d = e^(3d/r_s) · e^(-9d²/r_s²)` | thesis p. 58 (img 069) | IIIC.3 implementation | MATCH |
| 6 | Velocity transfer `g(v) = v/v_0` linear | thesis p. 58 (img 069): "fonction g est définie par une simple forme linéaire croissante: g(v) = v/v_0" | IIIC.3 with clamp `[0,1]` | MATCH (lab-named) — clamp at v_0 is a defensible boundedness choice; thesis is silent on what happens when v > v_0. P2-5 in the pre-IIID.3 triage flagged this as "scheduled before IIIH". |
| 7 | Elevation transfer `h(z̃) = z̃²` | thesis p. 58 (img 069) | IIIC.3 implementation | MATCH |
| 8 | Orogeny age reset on uplift `a_c(t+δt) = 0` | thesis p. 59 (img 070) | (Phase III crust state field; verify in IIIA fields wiring) | DEFERRED-TO-VERIFY (likely MATCH; not yet on critical path) |
| 9 | **Fold-direction subduction increment `f_j(t+δt) = f_j(t) + β·(s_i(p) − s_j(p))·δt`** | thesis p. 59 (img 070) | `CarrierLabVisualizationActor.cpp:8903-8905`: `OverVertex.FoldDirection + RelativeConvergence * DeltaKm` | **DIVERGE** — implementation scales the increment by **uplift magnitude `DeltaKm` (km)**, not by `β·δt` (dimensionless × time). Different physical quantity; implementation makes places of larger uplift get larger fold-direction change rather than uniform `β·δt` per timestep. Defensible heuristic, not the thesis formula. **Pre-IIID.3 triage P1-2 marked this as "scheduled before IIID collision uplift." It was implemented but in a form that does not match the thesis formula.** |
| 10 | Slab pull `w_i(t+δt) = w_i(t) + ε·Σ(c_i × q_k)/||c_i × q_k|| · δt` updated, then "normalisé après coup" | thesis p. 59 (img 070) formula uses `w_i` and "axe de rotation (normalisé après coup)" notation — ambiguous. **Thesis p. 63 (img 074) implementation prose disambiguates explicitly:** "*Ensuite nous mettons à jour l'**axe et la vitesse** du mouvement G_i de P_i pour refléter le Slab Pull exercé sur la plaque*" (axis AND speed are both updated). | `CarrierLabVisualizationActor.cpp:9255-9282`: `OldOmega = OldAxis × OldAngularSpeed; RawOmega = OldOmega + ContributionSum × SlabPullAngularStep`; decompose RawOmega into NewAxis + NewSpeed, clamp speed at v_0. Both axis AND speed change. | **MATCH** — page 63 explicit "axe et vitesse" prose disambiguates the page 59 formula notation. Implementation's coupled axis+speed update is paper-faithful. **The pre-IIID.3 triage's P1-3 disposition citing page 63 was correct.** An earlier draft of this audit (commit history of this document) wrongly reclassified P1-3 as "page 63 unsupported"; that reclassification is rescinded. The audit drafter's initial re-read missed the "axe et la vitesse" sentence. |

### 3. Convergence detection and tracking (§3.3.1.3)

| # | Decision | Thesis source | Implementation | Verdict |
|---|---|---|---|---|
| 11 | Triangle suivi list initialized from boundary triangles | thesis p. 61 (img 072) | IIIB.1 ActiveBoundaryTriangles | MATCH |
| 12 | Distance-to-front overestimation `d(p,t+δt) = d(p,t) + s(p)·δt` | thesis p. 62 (img 073) | IIIB.2 | MATCH |
| 13 | Per-plate BVH built per timestep; ray cast through triangle barycenters | thesis pp. 61–62 (img 072–073): "construire une *Bounding Volume Hierarchy* (BVH)... à chaque pas de temps" | IIIB.3 SubductionMatrix via per-plate BVH ray queries | MATCH |
| 14 | Subducting source rejection: if T_i is itself in subduction, reject hit | thesis p. 62 (img 073) | IIIB.3+IIIC.1 (subducting marks gate evidence acceptance) | MATCH |
| 15 | Polarity authorization: triangle vertex check (3 vertices of T_i, 3 vertices of T_j) for ocean-cont vs cont-cont | thesis p. 62 (img 073) | IIIB.4 read-only polarity decisions on IIIB.3 matrix | MATCH |
| 16 | Active-list neighbor propagation continues as long as front is convergent | thesis p. 61 (img 072) — list "reste dans la liste tant qu'une convergence active subsiste" | IIIB.6 plate-local edge-neighbor propagation | MATCH |
| 17 | List reset at episodic remesh | thesis p. 61 (img 072): "intervalle associé au remaillage épisodique de la planète... après quoi elle est réinitialisée" | IIIB.1 ConvergenceTrackingResetSerial bumps at remesh | MATCH |

### 4. Continental collision concept (§3.3.1.2)

| # | Decision | Thesis source | Implementation | Verdict |
|---|---|---|---|---|
| 18 | Collision is a discrete single-timestep event, not continuous | thesis p. 59 (img 070): "une collision est un évènement discret... à un certain temps t, le terrane R arrête sa migration propre et se soude" | IIID.6 honors one-collision-per-step; defers additional groups | MATCH |
| 19 | Source plate is **oceanic** carrying terrane R, going toward **continental** opposing plate | thesis p. 59 (img 070): "Nous considérons ici une plaque océanique P_i comportant un terrane R, convergeant vers une plaque continentale P_j" | IIID.1 detects continental terranes from `CollisionCandidate` evidence (cont-cont in IIIB.4); IIID.6 transfers source-plate-continental triangles to destination plate | **MATCH (lab-named)** — the thesis's primary worked example is oceanic-plate-carrying-continental-terrane → continental-plate. Our implementation generalizes to cont-cont source-to-destination directly. The lab-named generalization is reasonable but should be flagged: the thesis's worked example does NOT cover cont-cont collision both being continental from the start. Worth confirming with a fixture where the source plate is mixed-material (oceanic with continental terrane embedded) before IIIH. |
| 20 | Influence radius `r = r_c · √(v(q)/v_0) · A/A_0` | thesis p. 60 (img 071): displayed `r = r_c √(v(q)/v_0) A/A_0` with sqrt bar appearing to extend over `v(q)/v_0` only | `CarrierLabVisualizationActor.cpp:441-443`: `Scale = (v/v_0) × (A/A_0); r = r_c × sqrt(Scale)` — i.e. **both v/v_0 and A/A_0 are inside the sqrt** | **AMBIGUOUS / likely DIVERGE** — the raster image is unambiguous to the human eye in showing the bar over `v(q)/v_0` only, but the implementation places both factors inside the sqrt. Mathematically: thesis-literal `r = r_c · √(v/v_0) · (A/A_0)` versus implementation `r = r_c · √((v/v_0) · (A/A_0)) = r_c · √(v/v_0) · √(A/A_0)`. Difference: a factor of `√(A/A_0)` vs `(A/A_0)`. For typical small terranes (A/A_0 << 1), implementation produces a **larger** radius than the thesis-literal reading. **Recommend: re-verify against the thesis source LaTeX or original PDF; if the bar truly covers only v/v_0, the implementation has a real formula error and should be corrected before IIIH long-horizon validation.** |
| 21 | Collision uplift `Δz(p) = Δ_c · A · f∘d(p,R)`, `f(x) = (1 − (x/r)²)²`, INSTANT (not integrated over δt) | thesis p. 60 (img 071) | `CarrierLabVisualizationActor.cpp:390-419` (`PhaseIIID7CollisionDistanceTransfer`, `PhaseIIID7CollisionDeltaKm`) | MATCH |
| 22 | Collision fold direction `f(p,t+δt) = (n × (p−q)/||p−q||) × n` (radial-from-centroid, projected on tangent) | thesis p. 60 (img 071) | IIID.7 `(n × normalize(p − q)) × n` per slice report | MATCH |
| 23 | Distance d(p, R) is to the terrane polygon (compact) | thesis p. 60 (img 071): "distance au terrane compacte" | IIID.7 uses minimum geodesic distance to transferred terrane vertices and triangle barycenters | **MATCH (lab-named)** — the slice report explicitly calls this a "discrete carrier approximation" and flags continuous polygon distance as an IIIE review item. Lab-named approximation, acceptable. |

### 5. Continental collision implementation algorithm (§3.3.1.3)

| # | Decision | Thesis source | Implementation | Verdict |
|---|---|---|---|---|
| 24 | Build per-adverse-plate triangle list from suivi | thesis p. 63 (img 074) step 1 | IIID.2 grouping by canonical plate-pair | MATCH |
| 25 | Accept collision iff at least one source triangle has interpenetration ≥ 300 km | thesis p. 63 (img 074) step 2 — "Pour chaque telle plaque P_j, nous acceptons la collision uniquement si au moins un triangle de P_i a dépassé le seuil minimum de collision, fixé à 300 km" + footnote 6 "0.1 dans l'implémentation" (300 km radius normalized) | IIID.2 grouping gate at 300 km | MATCH |
| 26 | Terrane detection: connected continental triangles, **also include enclosed inner-sea triangles** | thesis p. 63 (img 074) step 3a — "Un traitement spécifique est également ajouté afin d'inclure dans les terranes toute portion de mer intérieure" | IIID.1 mesh traversal with inner-sea inclusion | MATCH |
| 27 | Mass test: enough opposing continental mass relative to source terrane mass | thesis p. 63 (img 074) step 3b — "Si cette portion n'est pas assez étendue par rapport aux terranes calculés sous-arrêtons" — **does NOT specify a numeric threshold** | IIID.3 uses `DestinationMassThresholdRatio = 0.5` (50% threshold) | **MATCH (lab-named)** — thesis says "assez étendue" without a number. 50% is our chosen threshold and should be labeled as a lab parameter, not a thesis number. Worth source-checking whether the original Cortial implementation source code (if available) commits to a specific number. Until then, our 50% is a defensible labelled-lab choice. |
| 28 | Slab break: extract terranes from source via duplication and topology recompaction | thesis p. 64 (img 075) step 3c | IIID.4 dry-run plan; IIID.6 mutation | MATCH |
| 29 | Suture: augment destination mesh with terrane triangles; re-init convergence tracking on front | thesis p. 64 (img 075) step 3e | IIID.5 dry-run plan; IIID.6 mutation with boundary tracking reinitialized; convergence tracking invalidated | MATCH |
| 30 | One collision per timestep when multiple adverse plates simultaneously in collision | thesis p. 64 (img 075): "*si plusieurs plaques sont en collision, une seule sera réellement traitée par pas de temps; les autres traitées aux pas de temps suivants*" | IIID.6 deferred-valid-plans = 0 in single-plate fixtures; defers additional groups | MATCH |
| 31 | **Multi-plate breakage during collision** — case where one plate is in collision with multiple others and may need to itself break into sub-plates | thesis pp. 63–64 do not directly address this. Page 64 footnote 7 is about *reverse collision* (P_j collides with P_i within the same suivi pass), **not** about multi-plate breakage being out of scope. | not implemented in our code either | **GAP / NEEDS THESIS RE-READ** — an earlier draft of this audit cited footnote 7 as evidence the thesis explicitly leaves multi-plate breakage out of scope. That citation was wrong (the audit drafter conflated footnote 7 with separate text or misread). Without supporting thesis evidence, this remains an **open gap**: our implementation does not handle a plate simultaneously in collision with multiple adverse plates that may need to itself fragment. Classify as audit-flagged open gap (matches the 1×3 Pro audit's "triple-junction not exercised" finding); revisit before IIIH. **Do not cite this as thesis-limited until the relevant thesis section is found and quoted directly.** |

### 6. Collision uplift application (§3.3.1.3 step d, "Soulèvement")

| # | Decision | Thesis source | Implementation | Verdict |
|---|---|---|---|---|
| 32 | Uplift propagation: from intersected triangles, transitive closure on neighbors until influence radius reached | thesis p. 64 (img 075) step 3d — operates at granularity per triangle: from P_j mesh, starting from intersection points with P_i adverse terranes, applying transitive closure | IIID.7 `TransitiveClosureOnDestinationMesh` (slice report description) | MATCH |
| 33 | **Obduction continuous uplift** — thesis page 57 (img 068) says cont-cont obduction "se déroule jusqu'à son terme" if no collision triggers; thesis page 58's `h(z̃) = z̃²` captures the impact-factor differentiation (continental >> oceanic) automatically | thesis pp. 57–58 (img 068–069) | **Cont-cont contacts get a `CollisionCandidate` label from IIIB.4 (verified at `CarrierLabVisualizationActor.cpp:2628-2630`), then `CollisionCandidate` is explicitly skipped at `CarrierLabVisualizationActor.cpp:2763-2767` (treated like `Ambiguous`). `IsSubductingDecision()` at `:678-682` returns true only for `OceanicUnderContinental` or `OlderOceanicUnderYoungerOceanic`, **excluding** `CollisionCandidate`.** Therefore cont-cont contacts do NOT produce subducting marks and never enter the IIIC.3 uplift path. | **GAP (real, control-flow)** — the `h(z̃) = z̃²` impact-factor argument from an earlier draft of this audit was correct in isolation but mis-applied: the formula's impact-factor differentiation is moot if cont-cont contacts never reach the IIIC.3 uplift path in the first place. **Per thesis: cont-cont obduction should accrue continuous uplift via the §3.3.1.1 subduction formula until/unless collision threshold is reached.** Our implementation produces NO uplift for cont-cont contacts that are sub-300-km (and therefore sub-collision-threshold) — they are obduction-pending but currently neutral. **P1-1 is a real gap**, not "addressed latent" as an earlier draft of this audit claimed. The pre-IIID.3 triage's "scheduled before IIID collision uplift" classification stands. |

### 7. IIID→IIIE handshake (§3.3.2.3 — IIIE territory)

| # | Decision | Thesis source | Implementation | Verdict |
|---|---|---|---|---|
| 34 | Pre-treatment Step 1: recalculate boundary triangles **excluding subducted triangles** | thesis p. 68 (img 079) §3.3.2.3 step 1 | not implemented (IIIE.5) | DEFERRED-TO-IIIE |
| 35 | Pre-treatment Step 1: **destroy plate if entirely subducted** | thesis p. 68 (img 079) §3.3.2.3 step 1 | not implemented (IIIE.5) | DEFERRED-TO-IIIE |
| 36 | Pre-treatment Step 1: rebuild BVH | thesis p. 68 (img 079) | not implemented (IIIE.5) | DEFERRED-TO-IIIE |
| 37 | Échantillonnage Step 2a: barycentric copy from intersected plate triangle, **filtering subducting/colliding triangles before ray hit** | thesis p. 68 (img 079) | not implemented (IIIE.3) | DEFERRED-TO-IIIE |
| 38 | Échantillonnage Step 2b: divergent zone q1/q2 spherical-midpoint-on-frontier ridge approximation | thesis p. 68 (img 079) | Stage 1.5 endpoint+midpoint q1/q2 (named lab approximation) | DEFERRED-TO-IIIE — IIIE will replace Stage 1.5 lab approximation with continuous q1/q2 |
| 39 | Multi-hit unresolved: **fail-loud after filter**, no fallback policy | thesis p. 68 (img 079) — thesis does not specify a multi-hit fallback (degenerate case) | Stage 1.5 lab policies (Centroid/Random/Synthetic/PriorOwner) — diagnostic only, gated to zero in IIID via `PolicyResolvedMultiHitCount==0` | DEFERRED-TO-IIIE — IIIE.3 must fail-loud rather than tiebreak |

### 8. Cadence (§3.3.2.3 implementation cadence formula)

| # | Decision | Thesis source | Implementation | Verdict |
|---|---|---|---|---|
| 40 | Episodic remesh cadence `ΔT = (1−α)M + αm`, `α = min(1, v_m/v_0)`, `m = 32 Ma`, `M = 128 Ma` | thesis p. 68 (img 079) | observed-speed cadence at commit `8aa1ac7`; flag `bEnableNaturalResamplingEvents`, default OFF | MATCH (the formula is implemented; defaults preserve prior behavior) |

## Summary Of Findings By Severity

### Blocker before IIIH long-horizon validation

- **Finding 9 (DIVERGE):** fold-direction subduction increment scales by `DeltaKm` (uplift magnitude), not by `β·δt`. Implementation at `CarrierLabVisualizationActor.cpp:8903-8905`. The IIIC.3 commandlet only gates `MinFoldMagnitude > 0`, not the thesis fold-formula content. Should be corrected to `β·(s_i − s_j)·δt` with a documented `β` constant, or explicitly named as a lab heuristic in `carrier-design.md` and `phase-iii-paper-process-design.md`.
- **Finding 20 (DIVERGE):** influence radius implementation places `A/A_0` inside the sqrt; thesis page 60 bar covers only `v/v_0`. Implementation at `CarrierLabVisualizationActor.cpp:441-443`. Difference is a factor of `√(A/A_0)` vs `(A/A_0)` — for typical small terranes (`A/A_0 << 1`) implementation produces a larger radius than thesis. Correct to `r_c · sqrt(v/v_0) · (A/A_0)` before any quantitative paper-comparison runs.
- **Finding 33 (GAP, real):** cont-cont contacts get `CollisionCandidate` from IIIB.4, then `CollisionCandidate` is explicitly skipped from the subducting-mark path (`CarrierLabVisualizationActor.cpp:2763-2767`, gated at `:678-682`). Therefore cont-cont obduction never reaches IIIC.3 uplift. Per thesis page 57: cont-cont obduction "se déroule jusqu'à son terme" if no collision — should accrue continuous uplift via §3.3.1.1 formula. Currently produces no uplift for sub-300-km cont-cont contacts. **P1-1 is a real gap; the pre-IIID.3 triage's "scheduled before IIID collision uplift" classification stands and was not actually addressed.**

### Lab-named items to label more honestly (not blockers)

- **Finding 27 (MATCH lab-named):** 50% destination-mass threshold is our chosen number. Thesis page 63 says "assez étendue" without a number. Already correctly labeled at `phase-iii-slice-plan.md:387` as "config; initial value: 50%"; just confirm the label is clear in IIID.3 design.
- **Finding 23 (MATCH lab-named):** IIID.7 distance-to-terrane discrete approximation. Already labeled by the IIID.7 slice report as a "discrete carrier approximation" with continuous polygon distance flagged as IIIE review item.

### Open audit gap (was over-classified in earlier draft)

- **Finding 31 (GAP):** multi-plate breakage during collision (one plate in collision with multiple adverse plates that may itself need to fragment). An earlier draft of this audit claimed thesis footnote 7 (page 64) explicitly leaves this out of scope; that citation was wrong (footnote 7 is about reverse-collision acceptance ordering, not multi-plate breakage). Without supporting thesis evidence, treat as audit-flagged open gap, not "thesis-limited." Revisit before IIIH long-horizon work; either find the relevant thesis discussion and quote it directly, or accept this as our gap relative to thesis-faithful continental tectonics.

### Confirmed match (high confidence)

- All §3.3.1.1 subduction triggering rules (rows 1–3).
- IIIC.3 subduction uplift formula and components (rows 4–7) — for ocean-continental and ocean-ocean only; cont-cont obduction does not reach this path (Finding 33).
- All §3.3.1.3 convergence detection mechanics (rows 11–17).
- **Slab pull axis+speed coupled update (row 10) — verified MATCH** via thesis page 63 explicit "axe et la vitesse" prose; an earlier draft of this audit reclassified incorrectly, that reclassification is rescinded.
- Collision discrete-event semantics, terrane detection with inner-sea inclusion, slab-break, suture, one-collision-per-step (rows 18, 26, 28–30).

### Deferred to IIIE (cannot be evaluated until §3.3.2.3 lands)

- All rows 34–39: IIIE.3 filtered ray-cast échantillonnage, IIIE.5 plate rebuild + fully-subducted destruction + BVH rebuild, IIIE q1/q2 continuous, fail-loud unresolved multi-hit.

## Audit Revision Note (2026-05-06)

This document was reviewed by Codex and returned with three valid pushbacks against the original draft's reclassifications. The pushbacks were correct on all three counts:

1. **Slab pull P1-3** — page 63 (img 074) **does** explicitly say "*Ensuite nous mettons à jour l'axe et la vitesse du mouvement G_i de P_i pour refléter le Slab Pull exercé sur la plaque*" (axis AND speed). The original audit drafter's re-read missed this sentence. Pre-IIID.3 disposition stands; row 10 verdict corrected to MATCH.
2. **Footnote 7** — page 64's footnote 7 is about reverse-collision acceptance ordering, not multi-plate breakage out of scope. The original audit drafter misquoted or hallucinated the "ce cas très particulier a été laissé en marge" text; it does not appear on page 64. Row 31 verdict corrected from THESIS-LIMIT to GAP.
3. **Obduction P1-1** — the `h(z̃) = z̃²` impact-factor argument is moot if cont-cont contacts never reach the IIIC.3 uplift path. Verified at `CarrierLabVisualizationActor.cpp:678-682` and `:2763-2767`: `CollisionCandidate` labels are excluded from the subducting decision filter, so cont-cont obduction-pending contacts produce no uplift. Row 33 verdict corrected from MATCH (latent) to GAP (real, control-flow).

**Meta-lesson:** the audit's original three reclassifications repeated the same class of error the audit was meant to catch — overconfident page-image claims based on incomplete re-reading. The user's standing discipline ("page-image inspection is the only authority on thesis-faithfulness") was applied imperfectly by the audit drafter on these three rows. The two real divergences (Findings 9, 20) and the lab-named labels (Findings 23, 27) survived the review unchanged. Future audits should: (a) quote thesis text verbatim with image-filename citation rather than paraphrasing from memory; (b) when overturning a prior verdict, re-read the cited page(s) end-to-end before drafting, not only the section the prior verdict named; (c) when claiming "X is captured implicitly by Y," verify the actual control-flow path that connects X's input to Y, not only the math.

## Confidence Statement (post-revision)

**Pre-audit confidence:** medium.

**Post-audit confidence (post-Codex-review):** **medium-high on convergence detection and collision topology mechanics** (rows 11–17, 18, 24–32 minus Finding 31, plus row 10 slab pull restored to MATCH after revision). **Medium on the uplift path**, because two real divergences (Findings 9, 20) and one real gap (Finding 33, cont-cont obduction never reaches IIIC.3) all sit on the elevation/orogeny-evolution surface that IIIH long-horizon validation will exercise.

**The remaining medium-confidence area beyond the uplift path is the IIID→IIIE handshake** (rows 34–39). That's not a defect of IIID; it's because the consuming algorithm doesn't exist yet. IIIE.3 will be the first time IIID's outputs (filtered subducting/colliding triangle marks, mass-conserved terrane transfer) get exercised end-to-end against a paper-faithful remesh.

## Recommendations

### Before IIIE entry

1. **Resolve Finding 9 (fold-direction divergence).** Either (a) correct implementation to `f_j(t+δt) = f_j(t) + β·(s_i − s_j)·δt` with a chosen and documented `β` constant; or (b) keep the current `DeltaKm`-coupled heuristic but name and justify it explicitly in `carrier-design.md` and the IIIC.3 design doc. Recommend (a).
2. **Resolve Finding 20 (influence radius sqrt scope).** Correct `CarrierLabVisualizationActor.cpp:441-443` to `r_c · sqrt(v/v_0) · (A/A_0)` to match thesis page 60.
3. **Resolve Finding 33 (cont-cont obduction never reaches uplift path).** Per thesis page 57, cont-cont obduction should accrue continuous uplift via the §3.3.1.1 formula until/unless 300 km collision threshold triggers IIID.7 instant uplift. Currently `CollisionCandidate` labels are skipped at `CarrierLabVisualizationActor.cpp:2763-2767` and `IsSubductingDecision()` at `:678-682` excludes them. Decision options: (a) extend the subducting-decision filter to also accept `CollisionCandidate` cases for IIIC.3-style continuous uplift while still emitting collision evidence to IIID; (b) add a separate "obducting" mark class that consumes the same IIIC.3 uplift formula; (c) explicitly label as deferred to IIIH/IIIH.1 and accept that pre-collision cont-cont contacts produce no uplift in the current implementation. (a) and (b) are paper-faithful; (c) is a deferred lab choice that should be flagged in `carrier-design.md`.
4. **Add fixtures before IIIH** — not before IIIE entry — for: (a) cont-cont collision where source plate is mixed-material (oceanic with embedded continental terrane, the thesis's primary worked example); (b) edge-case suture topologies (terrane creating a hole, non-simply-connected, multi-seam); (c) inner-sea inclusion exercise; (d) triple-junction / multi-plate breakage (Finding 31 — currently classified as audit-flagged gap, not thesis-limited). These were already on the IIID consolidation calibration list.

### Not blocking

- The 50% mass threshold (Finding 27) is already correctly labeled at `phase-iii-slice-plan.md:387` as "config; initial value: 50%"; no action needed.
- IIID.7 distance-to-terrane discrete approximation (Finding 23) is already explicitly labeled in the IIID.7 slice report.
- Slab pull (Finding 10) is verified MATCH after revision; no action needed. Pre-IIID.3 P1-3 disposition stands.

## Decision

Promote Phase IIID confidence to **medium-high on convergence detection and collision topology mechanics**, and to **medium on the uplift path**, conditional on Findings 9, 20, **and 33** being resolved before IIIE entry. The remaining IIID→IIIE handshake (rows 34–39) cannot be promoted until IIIE lands; that's IIIE's own audit territory.

Recommended next move from the original audit draft: address Findings 9, 20, and 33 as a focused thesis-faithfulness cleanup tranche before implementation work depends on those surfaces. This recommendation is preserved as audit context; see the Current Disposition section for the live sequencing note.

Triple-junction (Finding 31) is **not** a Pre-IIIE.5 blocker — it's flagged for before IIIH along with the other multi-plate fixture work.
