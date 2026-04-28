# Paper Carrier Extraction

Status: pre-coding deliverable. No production code is approved by this document.

## Scope And Source Handling

This note extracts the carrier-relevant specification from Cortial et al.,
"Procedural Tectonic Planets" (2019), with cross-checks against Cortial's
PhD thesis where the implementation details are clearer.

Source corpus used:

- `C:\Users\Michael\Documents\Unreal Projects\Aurous\docs\ProceduralTectonicPlanets\ProceduralTectonicPlanets.pdf`
- `C:\Users\Michael\Documents\Unreal Projects\Aurous\docs\ProceduralTectonicPlanets.txt`
- Cortial thesis PDF from the Aurous docs tree, extracted for this lab to
  `Intermediate\CarrierLabResearchExtracts\Cortial-thesis.layout.txt`

Copyright note: the original prompt requested every relevant sentence quoted.
This clean-room note instead uses short anchor phrases and page/section
references, then paraphrases the specification. That preserves reviewability
without reproducing long passages from the paper or thesis.

## Carrier Specification Table

| Topic | Source locus | Anchor | Confidence | Extraction |
| --- | --- | --- | --- | --- |
| Planet state | Paper sec. 3, fig. 4 | "set of tectonic plates" | clear | A planet is modeled as a collection of tectonic plates over a sphere. Oblateness is explicitly out of scope for the tectonic model and can be applied later as a warp. |
| Lithosphere partition | Paper sec. 3, tectonics fundamentals | "Plates form a partition" | clear | Plates partition the lithosphere conceptually. In the paper's model, they are not merely labels on a fixed output texture; they are moving crustal domains. |
| Plate authority | Paper sec. 3, model | "portion of crust" | clear | Each plate is a crust domain with its own crust fields. That plate-local domain is the authoritative tectonic carrier. |
| Plate-local fields | Paper table 1, thesis table 3.1 | xC, e, z, ao, r, ac, o, f | clear | Crust fields are defined on plate domains: crust type, thickness, elevation, oceanic age and ridge direction, continental orogeny type/age and fold direction. |
| Numerical representation | Paper sec. 3; thesis sec. 3.2.3 | sample points plus interpolation | clear | The implementation stores crustal and tectonic data on sampled points of spherical triangulations and evaluates intermediate points by barycentric interpolation. |
| Rigid motion | Paper sec. 3; thesis sec. 3.2.2 | "rigid geodetic movement" | clear | A plate moves by a rotation about an axis through the planet center. Material and direction fields attached to that plate must rotate with the plate. |
| Surface velocity | Paper sec. 3 | formula s(p) = omega w cross p | clear | The analytic motion law is available and must be used for Stage 1 drift checks. A material centroid should advect by this rotation, not by sample relabeling. |
| Initial automatic plates | Paper workflow; thesis sec. 3.2.4 | Voronoi cells | clear | The automatic initial state distributes plate centroids on the sphere and builds spherical Voronoi-like plate regions, optionally warped by coherent noise. |
| Initial manual plates | Thesis sec. 3.2.4 | projected maps | clear | The thesis also supports user-specified maps for plate partition, crust type, elevation, and plate motions. CarrierLab should not need this for the falsification baseline. |
| Plate-local mesh construction | Thesis sec. 3.2.4 | duplicate, reindex, compact | clear | The thesis says the global spherical Delaunay triangulation is copied into separate per-plate triangulations by duplicating and reindexing topology. This supports independent moving plate meshes. |
| Time step | Paper sec. 3; thesis sec. 3.3 | 2 My | clear | The paper implementation uses a geological time step of 2 million years. CarrierLab can use this as the default comparison cadence, while also allowing smaller diagnostic steps if explicitly documented. |
| Convergence detection | Paper sec. 6; thesis sec. 3.3.1.3 | boundary triangle intersections | clear | Plate collisions are detected through intersections of plate boundary triangles accelerated by per-plate BVHs. This is geometric interaction, not a persistent sample owner rule. |
| Convergence distance | Paper sec. 6; thesis sec. 3.3.1.3 | overestimated distance | clear | The thesis tracks distance to a convergence front as an approximate state variable advanced by motion, rather than recomputing exact nearest-front distances every time. |
| Subduction authority | Paper sec. 4.1; thesis sec. 3.3.1.1 | rules by crust/age | clear | Subduction/obduction is chosen by local/global plate configuration: old oceanic under young oceanic, oceanic under continental, forced obduction for continent-continent until collision. |
| Collision transfer | Paper sec. 4.2; thesis sec. 3.3.1.2 | terrane sutures | clear | Continental collision is a discrete event: connected continental terranes detach from their carrying plate and are attached to the collided plate. |
| Divergent gap treatment | Paper sec. 4.3; thesis sec. 3.3.2.1 | oceanic ridge | clear | Regions opened between diverging plates are filled with new oceanic crust whose elevation blends an interpolated plate-border elevation and a ridge profile. |
| New ocean ownership | Paper sec. 4.3; thesis sec. 3.3.2.3 | nearest plate | clear | New oceanic samples are assigned to the nearest existing plate after computing their ridge-relative fields. |
| Global resampling cadence | Paper sec. 6 | "global resampling every 10-60 iterations" | clear | The paper does not remesh continuously every step. It lets plates move, then periodically resamples the global triangulation and rebuilds plate meshes. |
| Global resampling source | Paper sec. 6; thesis sec. 3.3.2.3 | barycentric transfer | clear | For each global sample during resampling, if it intersects an existing plate, crust fields are transferred by barycentric interpolation from that plate's current geometry. |
| Global resampling misses | Thesis sec. 3.3.2.3 | no valid intersection -> divergence | ambiguous | The thesis treats no valid plate intersection as a divergent region and creates oceanic crust. It does not separately define numerical misses, topology holes, and real divergent gaps. CarrierLab must separate those classes. |
| Global resampling multi-hits | Paper sec. 6; thesis sec. 3.3.2.3 | not specified | ambiguous | The paper and thesis do not give a complete rule for multiple valid intersections at one global sample. CarrierLab must log raw candidate count, deterministic winner, and overlap classification separately. |
| Boundary degeneracy | Paper/thesis by implication | not specified | ambiguous | Global samples may lie exactly on edges or vertices of plate meshes. The source texts do not define whether those zero-area degeneracies are overlaps. CarrierLab must define a half-open/tie-break rule before coding. |
| Projection/output | Paper sec. 5 and 6 | crust data to relief | clear | The final high-resolution terrain is downstream of crust simulation. Output projection is not the tectonic authority. |
| Validation claim | Paper sec. 7 | 60k to 500k samples | clear | The paper reports performance at 60k, 100k, 250k, and 500k samples, with 40 plates and 0.3 land coverage for the table. It validates perceived realism, not the coverage/mass invariants CarrierLab must test. |

## Interpretations For CarrierLab

1. The clean-room carrier must use moving plate-local crust geometry as the
   authority. A fixed global sample lattice can be a resampling target or a
   diagnostic projection surface, but it cannot be the persistent material
   carrier during rigid motion.

2. The paper's carrier is not simply "nearest rotated plate center." That is
   Aurous Prototype C's replacement architecture, not the paper. The paper
   uses plate-local triangulations, boundary intersection, and periodic
   global resampling.

3. Periodic global resampling is part of the paper-faithful carrier, but it is
   also the most dangerous ambiguity. It must be implemented as a falsifiable
   operation with explicit miss, overlap, transfer, and mass diagnostics.

4. Rigid plate motion should be analytically checkable. Any continental
   material attached to a plate has an exact rotation from step 0 to step N.
   Stage 1 drift coherence should compare projected material centroids against
   this independent analytic rotation.

5. The paper does not define CarrierLab's required projection coverage gates.
   Those gates are lab requirements added to distinguish geometry holes,
   numerical query errors, real divergent openings, and overlap/convergence.

## Ambiguities To Resolve Before Code

- Boundary samples: decide whether exact edge/vertex degeneracy counts as a
  true overlap or as a deterministic half-open ownership tie. The raw candidate
  count should still be reported.
- Multi-hit winner: define a deterministic winner for projection output while
  preserving the raw multi-hit diagnostic.
- Miss classification: distinguish real divergent openings from numerical
  misses and invalid topology holes. Treating all misses as oceanic crust would
  reproduce a known failure shape.
- Resampling cadence: start with paper cadence, but record the selected
  interval and do not tune it after seeing failures without a written gate
  revision.
- Category transfer: crust class should be represented as either a material
  weight or an independently checked categorical interpolation rule. Do not
  let nearest-category interpolation silently erode continental mass.
