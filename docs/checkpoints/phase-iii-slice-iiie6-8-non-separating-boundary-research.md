# Phase IIIE.6.8 Non-Separating Boundary Research

## Status

needs-user-decision.

The sources resolve the narrow research question, but they do not specify a record-building action for a zero-hit sample whose nearest q1/q2 boundary pair has non-positive signed separating velocity. IIIE.6.9 should therefore not be framed as implementing a thesis-cited rule unless the user first approves a named lab policy for this edge case.

## Source 0 result — CarrierLab symptom

Source 0 is implementation symptom evidence only. It is not paper authority.

- IIIE.6.7 is PASS / DIAGNOSTIC ONLY: it classifies the live-apply vertex-record-builder invalid-record gate without changing remesh behavior.
- Both `manual_step_60_apply_record_builder` and `manual_step_60_replay_apply_record_builder` report selection resolved/gap/unresolved `59079/40921/0`.
- Both scenarios report invalid records `19512`, primary sum `19512`, generated/applied/rift `21409/21409/0`.
- The invalid-record reason distribution is only non-separating: noBoundary/nonsep/other `0/19512/0`; invalid assigned `0`; unhandled `0`.
- Process-state cross-reference is process any/sub/obd/coll `0/0/0/0`, so the observed invalid records are not currently explained by subduction/obduction/collision process marks.
- Timing shows record building is the expensive step: apply warmup/selection/record build/boundary-pair query/resolved-copy/validation/total `6.462/0.517/315.933/315.881/0.004/0.016/323.473`; replay `6.353/0.525/242.745/242.703/0.004/0.012/250.184`.
- Timing conclusion: the diagnostic stops before topology rebuild; minutes-scale editor apply cost is record building/gap-field validation, dominated by continuous boundary-pair query, not downstream topology rebuild.

## Source 1 result — thesis re-read

### §3.1.2 Preliminary modes, image `cc5c6807-054.png`

Relevant because the secondary question asks whether the thesis mentions transform-like behavior. The thesis explicitly names shear/transform as a third tectonic mode, using the term "décrochement transformantes", and then states that those zones are set aside from the modeled relief-producing modes. Classification for divergent-generation eligibility: explicit transform/shear background; silent as a §3.3.2.3 resampling rule.

Consequence: the thesis supports the geological/model distinction between divergence and transform/shear, but it does not say how a zero-hit remesh sample should be handled when the nearest q1/q2 pair is non-separating.

Search-note scope: transform/cisaillement/décrochement produced relevant background hits; vitesse relative appears on the convergence side; dorsale/rift/divergence appear in ocean/rift sections; glissement/coulissage/mouvement parallèle/ouverture/éloignement did not produce a resampling eligibility rule in the inspected pages.

### §3.2.4 Voronoi plate generation, images `cc5c6807-064.png` through `cc5c6807-066.png`

The plate-generation section describes plate-local triangulations, plate movements, and geometric plate boundaries. It references "frontières de plaques" and plate movements, including "mouvements G respectifs", but it does not introduce a boundary-type classifier for divergence/convergence/transform at the Voronoi boundary construction stage. Classification for divergent-generation eligibility: silent.

Consequence: no source support here for pre-labeling boundaries as spreading, transform, or non-separating before remesh ocean generation.

### §3.3.1 Convergence, images `cc5c6807-067.png` through `cc5c6807-075.png`

The convergence section classifies subduction, obduction, and collision and uses "vitesse de surface relative" in convergence-side elevation formulas. The implementation discussion detects convergence by plate overlap/intersection and treats triangles marked subducting or colliding as special during later resampling. Classification for divergent-generation eligibility: silent.

Consequence: convergence has process-state classes; transform-equivalent boundaries do not receive a symmetric process-state class in the required convergence pages.

### §3.3.2.1 Génération du plancher océanique, images `cc5c6807-076.png` and `cc5c6807-077.png`

The thesis conceptually ties oceanic crust generation to ridges separating "plaques divergentes" and to a point in a "zone de divergence". It gives the blend parameter from distances to the ridge and nearest divergent plate, then blends the generic ridge profile with interpolated plate elevation. Classification for divergent-generation eligibility: explicit conceptual divergent-domain requirement; silent on a record-builder signed-separation test.

Consequence: ocean generation is source-grounded only for divergent/ridge zones, but §3.3.2.1 does not define how to establish that a sample is in that domain.

### §3.3.2.3 Implementation, image `cc5c6807-079.png`

The thesis gives the actual resampling procedure. It says that if a global vertex intersects an existing plate, current crust data are assigned, excluding intersections with subduction/collision triangles. If "aucune intersection valide" is found, the thesis says the vertex is in a "zone de divergence". It then defines q1 as "q1 le plus proche" on a plate boundary and q2 as the second-nearest plate-boundary point on another plate. The text then proceeds to compute the new oceanic crust conceptually as in §3.3.2.1. Classification for divergent-generation eligibility: explicit zero-valid-hit-to-divergence rule; explicit geometric nearest-boundary q1/q2; silent on motion/separation filtering.

Consequence: the implementation text is not "find the nearest separating boundary pair." It is nearest geometric plate-boundary pair after the valid-intersection test fails.

### §3.3.3 Évolution océanique, images `cc5c6807-080.png` and `cc5c6807-081.png`

The thesis describes post-remesh plate coverage and oceanic evolution. It says oceanic crust evolves "depuis les dorsales" as it ages and densifies, then gives the temporal dampening formula. Classification for divergent-generation eligibility: implicit assumption that oceanic crust comes from ridges; silent on eligibility for creating oceanic samples.

Consequence: oceanic dampening applies after oceanic crust exists. It does not answer whether a non-separating zero-hit sample should have been created as oceanic crust.

### Thesis-only answers

| Question | Thesis-only answer |
|---|---|
| Primary: is zero valid hit unconditionally eligible, or is there a separation/boundary-type test? | §3.3.2.3 explicitly treats zero valid hit as divergence and proceeds to oceanic generation. §3.3.2.1 conceptually requires a divergent/ridge zone. No signed separation or boundary-type eligibility test is specified. |
| 1. q1/q2 geometric-only or motion-conditioned? | Geometric-only in the implementation text: nearest plate-boundary point and second-nearest point on another plate. Motion/separation is thesis silent. |
| 2. Transform-like behavior? | Explicit background mention of transform/shear in §3.1.2; those zones are set aside from modeled macro-relief. No resampling/ocean-generation transform rule is given in the required implementation pages. |
| 3. Non-positive q1/q2 separating velocity action? | Thesis silent. No skip, re-query, assign-existing, or generate-anyway branch is stated for that measured condition. |
| 4. Nearest separating boundary pair? | Not thesis explicit. At most thesis-implied as a possible guard for the conceptual "divergent" domain, but the actual q1/q2 text is nearest geometric boundary pair. |
| 5. Oceanic dampening assumptions? | Dampening assumes oceanic crust spreads/ages from ridges, but it applies to oceanic crust after creation. It does not define creation eligibility. |
| 6. Does thesis distinguish geometric no-hit, true divergent gap, and transform/non-separating boundary? | Background distinguishes tectonic modes; §3.3.2.3 does not distinguish these cases during remesh. It equates no valid intersection with divergence. |

## Source 2 result — CGF paper

### §4 Method and §4.3 Oceanic crust generation, image `aa42e52c-07.png`

CGF states that oceanic crust forms from the "ridge separating diverging plates". Its sampling description says new points are generated in the ocean-ridge region and assigned to the nearest plate. Classification: explicit conceptual divergent/ridge eligibility; silent on zero-hit q1/q2 separation testing.

### §4.5 Continental erosion / oceanic dampening, image `aa42e52c-07.png`

CGF oceanic dampening says crust "spreads from the mid-ocean ridge" and ages, increasing density and decreasing elevation. Classification: implicit post-creation ridge assumption; silent on creation eligibility.

### §5 Procedural amplification, image `aa42e52c-08.png`

CGF mentions "transform faults" as a visual/morphological feature around mid-ocean ridges in the procedural amplification section. Classification: explicit transform-fault mention for appearance; silent as a resampling or record-building rule.

Search-note scope: the relevant CGF transform hit is in procedural amplification, not §4.3 or §6; separation/relative-velocity language is present around convergence/elevation concepts, not zero-hit ocean generation.

### §6 Implementation Details, image `aa42e52c-08.png`

CGF says global resampling is performed every 10-60 iterations. The parameters of samples "between diverging plates" are computed by §4.3, while other samples are computed by barycentric interpolation from the intersected plate. Classification: explicit divergent-between-plates domain at a high level; silent on q1/q2, zero valid ray hits, signed separating velocity, and non-separating boundary-pair failures.

### Appendix A constants

Appendix A gives constants. It is silent on boundary-type eligibility, q1/q2 selection, transform handling, and non-positive separation.

### CGF-only answers

| Question | CGF-only answer |
|---|---|
| Primary | CGF explicitly limits ocean generation to ridge/diverging-plate regions, but it does not describe a zero-hit ray-cast rule or separation test. |
| 1. q1/q2 selection | CGF silent; q1/q2 are thesis implementation details, not stated in the paper. |
| 2. Transform-like behavior | CGF mentions transform faults as ridge morphology/procedural amplification, not as a remesh eligibility class. |
| 3. Non-positive separating velocity | CGF silent. |
| 4. Nearest separating boundary pair | CGF silent; not paper-explicit. |
| 5. Oceanic dampening | CGF assumes oceanic crust spreads from the mid-ocean ridge and then dampens with age; it does not define creation eligibility. |
| 6. Distinctions among no-hit/divergent/transform | CGF distinguishes samples between diverging plates from other samples at a high level, but it does not distinguish geometric no-hit from transform/non-separating no-hit. |

## Source 3 result — Driftworld

Driftworld is tertiary/non-authoritative comparison only.

`Saved/ExternalRefs/driftworld-tectonics/Doc/sections/SimpleTectonicModel.tex` describes oceanic crust generation and resampling as a simplified version of the original model. It says points outside plate triangles are handled by distances to "two nearest plates", assumes the ridge is halfway between those nearest points, and later says that if no plate is found, it creates a "new oceanic crust point". This is geometry-only in the documentation.

The same document has an overlap/ranking model for convergence-side decisions: plate score determines which plate goes under during overlap. That is not a separation filter for gap samples. A code/document search found many uses of `transform` as quaternion plate transforms and no transform-boundary class comparable to a modeled boundary mode. It also did not surface a rule for resampling samples whose nearest boundary pair is not separating.

Comparison result: Driftworld supports the existence of a geometry-only no-plate/new-ocean implementation lineage, but only as tertiary evidence. It cannot make the CarrierLab non-positive-separation policy paper-authoritative.

## Synthesis

| Question | Thesis | CGF | Driftworld | Consequence |
|---|---|---|---|---|
| Primary zero-hit eligibility | explicit rule for zero-hit -> divergence; implicit conceptual divergent/ridge domain; silent on signed separation | implicit support for divergent/ridge domain; silent on zero-hit rule | tertiary only geometry/no-plate -> ocean | Non-positive-separation handling is not source-specified. |
| q1/q2 geometric or separating | explicit rule: geometric nearest boundary pair | silent | tertiary only: nearest plates/vertices | "Find nearest separating boundary pair" is not thesis/CGF explicit. |
| Transform-like behavior | explicit background mode; resampling silent | explicit ridge-morphology mention; resampling silent | tertiary silent/no class found | Transform is source-recognized, but not a record-building branch. |
| Non-positive separating velocity action | silent | silent | tertiary only, appears ignored by geometry path | Skip/re-query/generate/assign-existing are all lab-policy choices. |
| Oceanic dampening assumption | implicit support after ocean exists | implicit support after ocean exists | tertiary only | Dampening cannot decide creation eligibility. |
| Distinguish no-hit, true divergent gap, transform/no-separation | contradicted by §3.3.2.3 implementation shortcut, which equates no valid hit with divergence | implicit high-level distinction only | tertiary only geometry path | The exact 19,512-case classification is underspecified by source authority. |

## Implication for the 19,512 cases

Ranked by source support:

1. Paper truly silent. This is the strongest source-grounded classification for the measured condition "nearest q1/q2 boundary pair has non-positive signed separating velocity." Thesis and CGF both state the surrounding divergent/ridge rule, but neither defines signed separating velocity in the remesh gap path or specifies skip, re-query, assign-existing, generate-anyway, or undefined behavior.

2. Transform/parallel-motion boundary handling. This is paper-cited as background: the thesis recognizes shear/transform as a third tectonic mode and says it is set aside from modeled macro-relief; CGF mentions transform faults only for ridge appearance. Applying that background to the 19,512 records would be lab policy, not a thesis implementation rule.

3. Zero-hit samples are not automatically divergent-generation domain. This is paper-implied by the conceptual "divergent plates/ridge" language, but it conflicts with the thesis §3.3.2.3 implementation shortcut that treats no valid intersection as a divergence zone. Treating zero-hit as a candidate domain that still needs eligibility testing would be a named lab policy resolving source underspecification.

4. q1/q2 should filter for separation. This is not paper-cited. It is at most weakly paper-implied by the divergent-domain concept and contradicted by the thesis's geometric q1/q2 wording. "Nearest separating boundary pair" is therefore CarrierLab-specific lab policy unless a new source is found.

## Recommendation for IIIE.6.9

IIIE.6.9 belongs to "design named lab policy because paper is silent," not "implement thesis-cited rule." The citation chain is: thesis §3.3.2.1 and CGF §4.3/§6 support ocean generation only for ridge/diverging-plate domains; thesis §3.3.2.3 explicitly makes zero valid intersection imply a divergence zone and chooses geometric q1/q2 boundary points; thesis §3.1.2 recognizes transform/shear zones but does not integrate them into the remesh record-building branch. Therefore the 19,512 non-positive-separation records need user-approved policy naming before any code change.

## Open questions

- No inspected thesis or CGF page defines signed separating velocity for a q1/q2 boundary pair.
- No inspected source says whether a non-positive-separation zero-hit sample should be skipped, re-queried, assigned existing crust, generated as ocean anyway, or left undefined.
- No inspected source says whether "no valid intersection" after filtering subducting/colliding triangles is an empirical proxy for true divergence or a strict semantic identity.
- The thesis background excludes transform/shear from modeled macro-relief, but the required implementation pages do not state how to detect or preserve such boundaries during remesh.
- Driftworld is consistent with geometry-only generation, but it is tertiary and simplified; it cannot settle CarrierLab policy.

## Summary for independent review

The thesis and CGF both ground oceanic-crust generation in divergent/ridge domains, but the thesis implementation page for resampling equates no valid plate intersection with a divergence zone and selects q1/q2 as nearest geometric boundary points on different plates. Neither source defines a signed-separation eligibility test, a transform/non-separating record class, or an action when nearest q1/q2 are non-separating. The CarrierLab 19,512 invalid records therefore expose a source silence rather than a missed thesis-cited branch; proceeding requires a named lab-policy decision.
