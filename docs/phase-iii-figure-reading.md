# Phase III Figure Reading

Status: first visual reread pass. This note records what the paper/thesis
figures imply for the next CarrierLab phase. It complements
`docs/phase-iii-paper-process-extraction.md`.

## Source Images Reviewed

Paper:

- `docs/ProceduralTectonicPlanets/aa42e52c-05.png`: plate model and Figure 5.
- `docs/ProceduralTectonicPlanets/aa42e52c-06.png`: Figures 6 and 7.
- `docs/ProceduralTectonicPlanets/aa42e52c-07.png`: Figures 8, 9, and 10.
- `docs/ProceduralTectonicPlanets/aa42e52c-10.png`: Figure 15 and Table 2.
- `docs/ProceduralTectonicPlanets/aa42e52c-11.png`: Figures 16 and 17.

Thesis:

- `docs/Synthèse de terrain à léchelle planétaire/cc5c6807-068.png`: Figure 36.
- `docs/Synthèse de terrain à léchelle planétaire/cc5c6807-069.png`: Figure 37.
- `docs/Synthèse de terrain à léchelle planétaire/cc5c6807-072.png`: convergence implementation.
- `docs/Synthèse de terrain à léchelle planétaire/cc5c6807-073.png`: convergence validation rules.
- `docs/Synthèse de terrain à léchelle planétaire/cc5c6807-076.png`: Figure 39.
- `docs/Synthèse de terrain à léchelle planétaire/cc5c6807-077.png`: Figure 40 and rifting start.
- `docs/Synthèse de terrain à léchelle planétaire/cc5c6807-078.png`: rifting probability and divergence implementation start.
- `docs/Synthèse de terrain à léchelle planétaire/cc5c6807-079.png`: divergence remesh algorithm.
- `docs/Synthèse de terrain à léchelle planétaire/cc5c6807-080.png`: remesh reconstruction and erosion start.

## Visual Readings

### Subduction Is A Front With Distance, Not Just A Pair Label

Figures 5 and 36 show subduction as an oceanic plate descending beneath an
overriding plate, with a front distance `d(p)`, accretionary wedge, trench, and
uplift on the overriding crust. Figure 37 shows uplift transfer curves keyed to
distance, relative speed, and subducting relief.

Design implication: a Phase III subduction process needs per-front or
per-triangle distance state. A plate-pair boolean is not enough; it can decide
permission, but not the local uplift or when a triangle has disappeared from
tectonic influence.

### Collision Is Terrane Suture, Not Winner Selection

Figure 7 shows a connected terrane on one plate detaching and merging into the
collided plate, with an influence region around the terrane. The visual model is
topological transfer plus uplift, not a samplewise resolver.

Design implication: same-material continental multi-hit should become a
collision candidate with connected terrane detection. A local centroid or
nearest-plate policy is only a diagnostic control.

### Divergent Gap Fill Is New Oceanic Crust

Figures 8, 9, 39, and 40 all show divergent zones filled by oceanic crust along
a ridge. The model uses distance to ridge and distance to plate to blend a ridge
profile with interpolated border elevation. The thesis explicitly depicts a
triple junction between divergent plates.

Design implication: the Slice 5 gap-fill bucket is likely the largest remaining
process-coupled area, not an arbitrary accounting bug. Phase III needs to prove
whether the samples in that bucket are genuinely new-ocean samples or samples
whose continental material should have survived through valid intersection,
collision, or rifting state.

### Rifting Is Discrete Fragmentation In This Paper

Figure 10 and Figure 16 depict fracture followed by separated plates. The
limitations section says the implementation uses direct divergence of fragments
instead of progressive tearing.

Design implication: a paper-faithful first rifting slice should split a plate
into duplicated/re-indexed sub-plates with divergent motion. Progressive
continent tearing is not first-slice paper faithfulness.

### Published Morphology Depends On Multiple Processes Together

Figure 15 shows a sequence: subduction creates seafloor uplift/island arc, user
motion changes the convergence direction, then collision/accretion attaches the
island to the continent. Figure 16 shows collision, subduction, collision after
massive continental crust, and later rifting producing a new sea. Figure 17
shows recognizable landforms after long evolution.

Design implication: Phase III should not judge visual success from any one
process alone. The paper's morphology emerges from ordered interaction:
convergence state, subduction, collision, episodic divergence remesh, and
discrete rifting.

## Visual Gate Suggestions For Phase III

- Subduction overlay: show front triangles, distance-to-front bands, subducting
  triangles, overriding uplift zone, and rejected intersections.
- Collision overlay: show candidate terranes, destination plate, suture target,
  influence radius, and topology transfer records.
- Divergence overlay: show gap samples, q1/q2 plate ids, ridge midpoint, ridge
  direction, and whether prior continental material existed at that global TDS
  sample.
- Rifting overlay: show fracture Voronoi cells, duplicated sub-plates, new
  geodetic axes, and post-split divergence vectors.
- Accounting overlay: classify every material delta as valid process event,
  unresolved process case, or numeric residual.

## Caution For Figure Matching

The figures are process evidence, not final-art gates for the current lab. The
paper's published visual outputs include relief amplification, erosion,
oceanic dampening, cloud/ocean rendering, and user-authored scenarios. Phase III
should compare carrier/process diagnostics to the diagrams first, not attempt
full visual reproduction prematurely.
