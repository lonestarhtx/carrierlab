# Phase III Paper Process Extraction

Status: first reread pass. This document anchors the next CarrierLab phase to
Cortial et al. rather than inventing new carrier fixes. It is not an
implementation plan and does not approve Phase III code.

Primary sources:

- `docs/ProceduralTectonicPlanets/ProceduralTectonicPlanets.pdf`
- `docs/Synthèse de terrain à léchelle planétaire/Synthèse de terrain à l'échelle planétaire.pdf`
- Page PNGs in the same folders, used for figures and diagrams.

## Scope Of This Extraction

Phase I and Phase II showed that the plate-local carrier, ray projection,
subduction labeling, and subducting-triangle resampling filter can work
cleanly. The remaining material accounting issue is concentrated in
single-hit transfer and divergent gap fill. This reread asks whether those are
carrier defects, missing process state, or paper-specified process behavior.

Current read: they are process-coupled. The paper/thesis expects convergence
and divergence processes to modify crust state before and during episodic
remeshing. Phase III should therefore reconstruct the missing paper processes
instead of adding generic preservation heuristics.

## Process Ordering

The thesis implementation section is more specific than the paper. The
operational order is:

1. At the beginning of each time step, apply each plate's geodetic rotation to
   every vertex in every plate triangulation. Rotate vector-valued crust
   parameters as well, including fold direction and local ridge direction.
2. Detect and track convergence from boundary triangles, expanding to active
   neighboring triangles as convergence continues.
3. Apply subduction/obduction locally per triangle, while using global
   plate-pair state to decide whether subduction is allowed.
4. Run a dedicated continental-collision pass after subduction processing.
5. Resolve divergence episodically by global remeshing at cadence
   `DeltaT = (1 - alpha)M + alpha m`, with `alpha = min(1, vm / v0)`,
   `m = 32 Ma`, and `M = 128 Ma`.
6. After global remeshing, reset subduction state because subducted triangles
   disappear during remeshing and the subduction matrix is rebuilt for later
   steps.
7. After remeshing and planet reconstruction, perform stochastic plate
   fragmentation/rifting tests.

Implication: Phase III should not treat resampling as a standalone carrier
operation. In the paper, remeshing is the divergence implementation and is
interleaved with convergence state.

## Subduction And Obduction

Paper Section 4.1 and thesis Section 3.3.1.1 define subduction as a convergent
interaction where oceanic crust goes under another plate. Oceanic-continental
polarity is direct: oceanic under continental. Oceanic-oceanic polarity is
age-based: older oceanic crust subducts. Continental material carried on an
oceanic plate is treated as a terrane/obduction case, not as ordinary oceanic
subduction.

In the thesis implementation, convergence detection casts rays from the planet
center through candidate triangle barycenters and tests other plates through
per-plate BVHs. Intersections are rejected when the encountered triangle is
already subducting. If the plate pair has no valid subduction relation, the
intersection is rejected. If both triangles are fully continental, the mode
switches to continental collision.

For accepted subduction/obduction:

- uplift is applied to the overriding plate;
- fold direction and orogeny state are updated on continental overriding
  crust;
- slab pull updates the subducting plate's geodetic motion;
- the subducting triangle and its vertices are marked as subducting;
- neighboring triangles are added to the active convergence frontier;
- subducting oceanic triangles at the front receive trench-style visible
  elevation while retaining historical elevation for later uplift calculation.

Implication: Phase II's triangle-label/filter stack matches only the
subducting-triangle exclusion part. Phase III needs the continuing state:
distance-to-front, subduction matrix, neighbor propagation, slab pull, and the
distinction between visible trench elevation and historical subduction
elevation.

## Continental Collision

Paper Section 4.2 and thesis Section 3.3.1.2 define continental collision as a
discrete event, not a continuous per-step process like subduction. It happens
after obduction/subduction when sufficient continental mass is involved.

The thesis implementation pass:

- groups collision triangles by opposing plate;
- requires a minimum interpenetration distance before accepting collision;
- detects connected continental terranes on the subducting/source plate;
- evaluates whether enough continental mass exists on the opposing plate;
- detaches the terrane from its source plate by duplicating and recompacting
  topology;
- propagates collision uplift across the destination plate by neighborhood;
- sutures the terrane triangles into the destination plate;
- processes only one such collision per time step to avoid topology edge cases.

Implication: same-material continental overlaps should not be resolved by a
centroid tie-break. They should become collision candidates with connected
terrane extraction, mass tests, and explicit topology transfer.

## Oceanic Crust Generation

Paper Section 4.3 and thesis Section 3.3.2.1 define divergent gap fill as
new oceanic crust generated along a ridge between diverging plates. This applies
between two or more divergent plates, independent of the crust type of the
separating plates.

For a point in the divergent region:

- compute distance to the ridge and distance to the nearest plate;
- blend interpolated border elevation with a generic ridge profile;
- compute local ridge direction from the projection of the point onto the
  ridge;
- assign oceanic crust parameters, including oceanic age and ridge direction.

The thesis Section 3.3.2.3 gives the operational approximation used in the
global remesh:

- if a global TDS vertex has a valid ray-triangle intersection, barycentrically
  copy current crust parameters from the intersected plate triangle;
- if no valid intersection exists, treat the vertex as a divergent zone;
- find the closest boundary point `q1`;
- find the closest boundary point `q2` on a different plate;
- approximate the ridge point as the spherical midpoint between `q1` and `q2`;
- assign new oceanic parameters from the oceanic-crust generation model;
- assign the new sample to the nearest existing plate.

Implication: gap fill overwriting continental material is not automatically a
bug if the sample genuinely lies in a divergent zone. It becomes a bug only
when carried continental crust should still have a valid plate-triangle
intersection or when a process such as rifting/collision should have transferred
or preserved the material before the divergent remesh.

## Plate Rifting

Paper Section 4.4 and thesis Section 3.3.2.2 define rifting as a discrete plate
fragmentation event. A plate is split into two to four sub-plates using
Voronoi-style cells inside the original plate, with warped fracture boundaries.
The new sub-plates are assigned random but globally divergent geodetic
directions.

Rifting probability depends on:

- a user/model constant;
- the plate's continental coverage ratio;
- the plate area relative to average initial plate area.

The thesis places stochastic rifting after global remeshing. Before destroying
the old plate, it partitions the original plate's triangles and creates
sub-plates by duplication, re-indexing, and topology recompaction.

Implication: Phase III should not implement rifting as continuous tearing. The
paper explicitly uses direct fragmentation/divergence for performance and notes
that progressive tearing is future work.

## What Phase III Should Not Invent

- No global ownership persistence or sample anchoring.
- No centroid tie-break as a primary process.
- No generic "preserve continent on gap" rule without proving how it maps to
  paper rifting, collision, or valid intersection state.
- No triple-junction collapse into two-plate labels. The oceanic generation
  figures include multi-plate divergent junctions, but the convergence model
  still needs explicit local contact evidence.
- No terrain-beauty elevation pipeline until process state is auditable.

## Phase III Research Questions

1. Does the remaining single-hit transfer loss come from incorrect
   barycentric/material interpolation, from missing vector/material fields, or
   from legitimate process mutation not yet represented?
2. In divergent gaps, can the q1/q2 oceanic-crust generation rule be applied
   exactly while still accounting for any continental material that should have
   been transferred by collision or fragmented by rifting first?
3. Does adding age-based ocean-ocean polarity reduce same-material unresolved
   overlaps without introducing hidden material loss?
4. Can continental collision be represented as explicit terrane extraction and
   suture while preserving the plate-local authority model?
5. Does the paper ordering, especially convergence before episodic divergence
   remesh, close the material accounting gap seen in Slice 5?

## Proposed Phase III Name

`Phase III: Paper Process Reconstruction`

This phase should not be framed as "fixing the carrier." It should be framed as
reconstructing the paper's process state around the already validated carrier.
