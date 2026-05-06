# Phase III Pre-IIIE Thesis Cleanup

Status: in progress. This checkpoint records the focused cleanup requested after
the IIID visual/performance validation pass. It does not start IIIE.1.

## Scope

This tranche addresses two local thesis-formula findings and makes one
obduction control-flow decision before any IIIE remesh-contract work begins:

- Finding 9: IIIC.3 fold direction used uplift magnitude as the increment
  scale.
- Finding 20: IIID.7 collision influence radius placed the terrane-area factor
  under the square root.
- Finding 33: continental-continental obduction currently reaches IIID
  collision evidence, but not the IIIC.3 continuous uplift path before the
  collision threshold.

No new carrier authority source, ownership retention, repair, recovery,
backfill, projection-derived state, or IIIE remesh implementation is introduced
by this cleanup.

## Thesis Sources

- Thesis page 59 (`docs/Synthèse de terrain à léchelle planétaire/cc5c6807-070.png`):
  fold direction updates as
  `f_j(t+δt) = normalize(f_j(t) + β · (s_i(p) - s_j(p)) · δt)`.
- Thesis page 60 (`docs/Synthèse de terrain à léchelle planétaire/cc5c6807-071.png`):
  collision uplift influence radius is
  `r = r_c · sqrt(v(q)/v_0) · A/A_0`.
- Thesis page 57 (`docs/Synthèse de terrain à léchelle planétaire/cc5c6807-068.png`):
  continental-continental contact enters obduction and becomes collision only
  if the opposing continental mass is significant.

## Cleanup Actions

### Finding 9: Fold Direction Formula

Decision: fix now.

Implementation shape:

- `PhaseIIICFoldDirectionBeta` is the explicit lab constant for the thesis
  `β` factor.
- The fold-direction increment is the raw tangent relative velocity step
  `(s_i - s_j) · δt` at the over-plate sample, multiplied by beta.
- The update no longer scales with uplift `DeltaKm`.
- The IIIC.3 commandlet recomputes the expected fold direction from audit
  record inputs and gates the max vector residual.

Expected claim after regeneration: IIIC.3 may claim the implemented fold
direction update matches the thesis page 59 formula for the exercised
subduction-uplift fixture.

### Finding 20: Collision Influence Radius

Decision: fix now.

Implementation shape:

- `PhaseIIID7InfluenceRadiusKm` now computes
  `r_c · sqrt(v(q)/v_0) · A/A_0`.
- The IIID.7 commandlet recomputes the radius from audited terrane area,
  relative velocity, reference velocity, and reference radius and gates the
  residual.

Expected claim after regeneration: IIID.7 may claim the influence-radius
formula matches the thesis page 60 reading for the exercised collision-uplift
fixture. It must still preserve the existing caveat that distance-to-terrane is
a discrete carrier approximation, not a continuous polygon-distance oracle.

### Finding 33: Obduction Continuous Uplift

Decision: do not fix by overloading subducting marks.

Rationale:

- `ConvergenceSubductingTriangleMarks` already feed rendering/projection
  exclusion, trench/elevation split, slab-pull, and IIIE remesh filtering.
- Continental-continental obduction-pending evidence is not the same semantic
  object as a true subducting triangle.
- Overloading subducting marks would risk making IIIE's filtered-remesh contract
  ambiguous at exactly the boundary where it needs to be most explicit.

Required next cleanup before IIIE.1:

- Add a separate obduction-pending evidence/mark path, or record an explicit
  user-approved deferral.
- The preferred bridge should consume IIIB `CollisionCandidate` evidence,
  apply the IIIC.3 continuous uplift formula for sub-threshold obduction, and
  leave IIID's 300 km collision/suture event as the discrete threshold path.
- A fixture should demonstrate sub-threshold continental-continental uplift
  without topology mutation.

Until that bridge or deferral lands, IIID must not claim full
continental-continental obduction behavior. It may claim the already-accepted
local collision/suture behavior for the exercised fixtures.

## Exit Criteria

- `CarrierLabPhaseIIIC3` passes with fold oracle residual at floating-point
  tolerance.
- `CarrierLabPhaseIIID7` passes with radius oracle residual at floating-point
  tolerance.
- The IIIE entry audit and Phase III design docs record Findings 9 and 20 as
  fixed, and Finding 33 as a separate obduction bridge blocker/defer point.
- IIIE.1 is not started by this tranche.
