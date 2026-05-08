# Phase III Architectural Deviation Verification

Audit target: commit `2684d95` (`2684d9581bde8bd31ba75f19685d8816bd7a27ad`) on `master`.

This checkpoint is research-only. It verifies the Cortial thesis / CGF paper data-structure contract and compares it with CarrierLab's current architecture. It does not specify an implementation slice.

## Status

UNCLEAR.

The architecture is not a clean deviation: the thesis and CGF paper both describe a precomputed global spherical Delaunay triangulation used to build per-plate triangulations, and CarrierLab has the same broad shape as global samples/triangles plus per-plate local copies. The unresolved part is narrower and load-bearing: neither the thesis nor the CGF paper gives an explicit same-distance, multi-valid ray-intersection tie-break for boundary-shared plate-local triangles.

So the 5,845 IIIE.6.2 holds are not evidence that CarrierLab invented a structurally impossible paper case. They are evidence that CarrierLab has reached a legitimate boundary ambiguity that the sources do not resolve explicitly.

## Source 1 result - thesis data structures and remesh implementation

The page images were read directly from `docs/Synthèse de terrain à léchelle planétaire/cc5c6807-NNN.png`; page numbers below use the supplied image-to-page offset.

**Global TDS plus per-plate triangulations.** Section 3.2 constructs a global spherical Delaunay triangulation from Fibonacci sphere samples. The thesis calls out a `TDS globale` on `cc5c6807-066.png` (p.55), then says the model extracts sub-triangulations, `une pour chaque plaque`, from it. The same page says creating plate structures involves `dupliquer, de ré-indicer` the copied topology into dedicated plate triangulations.

Read together with `cc5c6807-064.png` (p.53) and `cc5c6807-065.png` (p.54), the model is:

- A precomputed global spherical Delaunay triangulation over the sampling sphere.
- A plate setup step that partitions/copies global TDS triangles and vertices into one local spherical triangulation per plate.
- Local plate topology with empty neighbor links across plate boundaries, not a single shared mutable topology.

This means plate-local triangulations are disjoint in storage and adjacency, but not disjoint in geometric origin. Boundary triangles on neighboring plates can share the same global TDS vertices or a global TDS edge after duplication/re-indexing.

**Remesh ray-cast.** Section 3.3.2.3 (`cc5c6807-079.png`, p.68) reuses the global TDS as the remesh target. For each global TDS vertex, it casts a ray from the planet center through that vertex and tests against every existing plate BVH. It filters subducting and colliding triangles, then assigns crust data from the intersected triangle by barycentric interpolation and records `l’indice de la plaque intersectée`. If `aucune intersection valide` is found, the vertex is in a divergent zone and receives newly generated oceanic-crust parameters.

The next page (`cc5c6807-080.png`, p.69) then partitions the global TDS into new plate groups according to the recorded plate indices and duplicates/re-indexes those sub-triangulations into new plate-local triangulations. It also resets subduction tracking for the next window.

**Same-distance multi-valid case.** I found no explicit thesis rule for two or more valid intersections at the same ray distance. The text iterates over each plate, but then describes the valid-hit path in singular terms: one intersected triangle, one intersected plate index. It does not say first, closest, primary, owner, previous owner, lower id, random, or any equivalent tie-break for multiple equally valid boundary intersections. The later "nearest existing plate" record is described as an additional per-global-vertex memory, not as the valid-intersection winner rule.

**Source 4 cross-check.** `docs/paper-resampling-extraction.md` and `docs/phase-iii-paper-process-design.md` correctly flag the global-TDS/per-plate-local architecture, but they are stronger than the page images when they phrase the post-filter hit as "the remaining valid intersection" if read as an explicit uniqueness guarantee. The page images support "filter invalid process triangles, interpolate from a valid hit, zero-hit means divergence"; they do not prove a unique-hit invariant or a same-distance tie-break rule.

## Source 2 result - CGF 2019 paper

The CGF PDF and page images were read from `docs/ProceduralTectonicPlanets/ProceduralTectonicPlanets.pdf` and `docs/ProceduralTectonicPlanets/aa42e52c-NN.png`.

The paper is more condensed than the thesis. In §4, it says plates partition the lithosphere and are modeled as spherical triangulations carrying crustal and tectonic data (`aa42e52c-04.png`, `aa42e52c-05.png`). In §6 Implementation Details (`aa42e52c-08.png`), it states that initialization constructs a `global Spherical Delaunay Triangulation`, partitions it into initial plates, and later reuses the offline sampling/meshing for seafloor generation/remeshing. The same implementation page says non-divergent sample parameters come from barycentric interpolation of crust data from the `plate they intersect`, and the new plates are built from `samples assignments`.

I did not find a multi-valid same-distance ray-hit rule in §4.3 oceanic crust generation, §4.5 continental erosion/oceanic dampening, §6 implementation details, or Appendix A constants. The CGF paper confirms the same architecture at a higher level, but it does not add a first/closest/owner/primary tie-break that the thesis lacks.

## Source 3 result - CarrierLab actual architecture

CarrierLab maintains global samples and global triangles, plus per-plate copied local triangulations. It does not have a single class literally named "global TDS", but it is not "only global samples" either.

From `Source/CarrierLab/Public/CarrierLabCarrier.h:17-64`:

```cpp
struct FSphereSample
{
	int32 Id = INDEX_NONE;
	FVector3d UnitPosition = FVector3d::ZeroVector;
	double AreaWeight = 0.0;
	int32 PlateId = INDEX_NONE;
	...
};

struct FSphereTriangle
{
	int32 A = INDEX_NONE;
	int32 B = INDEX_NONE;
	int32 C = INDEX_NONE;
	int32 PlateId = INDEX_NONE;
	bool bBoundary = false;
};

struct FCarrierVertex
{
	int32 GlobalSampleId = INDEX_NONE;
	FVector3d UnitPosition = FVector3d::ZeroVector;
	...
};

struct FCarrierPlateTriangle
{
	int32 A = INDEX_NONE;
	int32 B = INDEX_NONE;
	int32 C = INDEX_NONE;
	int32 SourceTriangleId = INDEX_NONE;
	bool bBoundary = false;
};
```

From `Source/CarrierLab/Public/CarrierLabCarrier.h:71-83` and `:162-170`:

```cpp
struct FCarrierPlate
{
	int32 PlateId = INDEX_NONE;
	...
	TArray<int32> SampleIds;
	TArray<int32> TriangleIds;
	TArray<FCarrierVertex> Vertices;
	TArray<FCarrierPlateTriangle> LocalTriangles;
	TMap<int32, int32> GlobalSampleIdToLocalVertexId;
};

struct FCarrierState
{
	FStage0Config Config;
	TArray<FSphereSample> Samples;
	TArray<FSphereTriangle> Triangles;
	TArray<FCarrierPlate> Plates;
	...
};
```

`BuildColdStartCarrier` builds a spherical Delaunay convex hull into `State.Triangles`, then assigns plates and builds adjacency/local triangulations (`Source/CarrierLab/Private/CarrierLabCarrier.cpp:589-596`). `AssignPlatesAndTriangles` assigns each global sample to a plate and each global triangle to one plate, while marking triangles whose vertices span multiple plate ids as boundary triangles (`:383-417`).

`BuildPlateLocalTriangulations` resets each plate's local arrays, then copies each assigned global triangle into the owning plate using `FindOrAddLocalVertex` (`:447-517`). `FindOrAddLocalVertex` records the source global sample on the local vertex (`Vertex.GlobalSampleId = Sample.Id`) and deduplicates only inside that one plate (`:104-124`). Therefore the same global sample can appear as independent local vertices in multiple plates if adjacent plate-local triangles share that global source at a boundary.

Current IIIE query logic matches the paper's broad ray-cast shape. `QuerySampleCandidates` casts `FRay3d Ray(FVector3d::Zero(), Sample.UnitPosition)`, loops every plate, and calls each plate BVH's `FindAllHitTriangles` (`Source/CarrierLab/Private/CarrierLabVisualizationActor.cpp:1565-1612`). `RunPhaseIIIE3FilteredRemeshSelectionAuditForSamples` filters subducting, obduction-pending, and collision-pending triangles before selecting (`:6392-6479`). If more than one visible candidate remains, the current selector records an unresolved multi-hit after only within-plate duplicate coalescing (`:2353-2462`).

The IIIE.6.2 diagnostic classifies unresolved holds using the plate-local triangle's `SourceTriangleId` and each local vertex's `GlobalSampleId` (`:1978-2026`). It then distinguishes shared global edge, shared global vertex, three-plate common vertex, field mismatch, no-shared-vertices, and non-boundary/interior overlap (`:2118-2210`).

## Architectural comparison

| Question | Thesis / CGF source | CarrierLab at `2684d95` | Result |
| --- | --- | --- | --- |
| Global TDS exists? | Yes. The thesis and CGF both describe a precomputed global spherical Delaunay triangulation used for initialization and remeshing. | Yes in representation: `FCarrierState::Samples` plus `FCarrierState::Triangles`; not a separate TDS class. | Match in substance. |
| Plate-local triangulations exist? | Yes. The thesis duplicates/re-indexes sub-triangulations into dedicated plate-local triangulations. | Yes. `FCarrierPlate::Vertices` and `LocalTriangles` copy global triangle/vertex sources. | Match. |
| Boundary features shared? | Local storage is duplicated, but boundary vertices/edges originate from the same global TDS. Empty neighbors at plate borders are topological, not geometric separation. | Local storage is duplicated, while `GlobalSampleId` / `SourceTriangleId` preserve shared global origin. | Match in the relevant sense. |
| Remesh ray tests every plate? | Yes. §3.3.2.3 tests every existing plate BVH for each global TDS vertex. | Yes. `QuerySampleCandidates` loops every plate BVH for each sample ray. | Match. |
| Subducting/colliding filtering? | Yes. Thesis excludes those intersections before accepting a hit. | Yes for subducting, obduction-pending, and collision-pending sets in the IIIE.3 path. | Match / lab extension for obduction-pending naming. |
| Multi-valid same-distance tie-break? | Not found. Sources use singular hit language but do not specify what to do if multiple valid boundary hits remain. | Current code fails loud as unresolved cross-plate/third-plate multi-hit unless a separate policy winner is later introduced. | Paper-silent; lab policy needed. |

## Implication for the 5,845 cross-plate holds

The IIIE.6.2 distribution is consistent with the paper's architecture rather than refuting it. The paper architecture creates duplicated per-plate triangulations from a shared global TDS. A ray through a global TDS vertex or edge can therefore touch valid boundary features from more than one plate-local triangulation at the same distance.

However, the sources do not provide a thesis-cited winner rule for:

- 3,740 two-plate shared global-edge holds.
- 2,021 two-plate shared global-vertex-only holds.
- 84 three-plate common global-vertex holds.

The absence of field mismatch, no-shared-vertices, and non-boundary/interior overlap means these are not currently evidence of a CarrierLab tracking bug or an impossible overlap class. They are valid boundary ambiguities. But because the thesis does not state a tie-break, IIIE.3 cannot honestly claim that resolving these by first hit, closest plate, prior owner, plate id, centroid, random, or any other winner rule is thesis-cited.

The selector consequence is therefore narrow: shared-edge, shared-vertex, and three-plate-vertex holds need an explicitly named and disclosed lab policy before they can be resolved in the primary IIIE remesh path. Until that approval exists, treating them as counted unresolved boundary ambiguities is the only source-faithful behavior available from the consulted pages.

## Open questions

- The thesis does not state whether the implementation relied on floating-point iteration order, CGAL/BVH hit ordering, or a hidden uniqueness assumption to collapse same-distance boundary hits. The page images do not resolve that.
- The thesis mentions storing the nearest existing plate for each global vertex after the valid/no-valid branch, but does not define that record as a valid-hit tie-break. Its exact downstream use in divergent-zone generation would need separate verification before using it as policy evidence.
- The page images establish duplicated/re-indexed plate-local triangulations, but do not provide pseudocode for assigning a global TDS triangle to a new plate when its three vertices carry mixed post-remesh plate indices. The text says partition by assigned plate indices; the detailed mixed-triangle partition rule remains underspecified from the consulted pages.

## Recommendation

The next IIIE.6.3 slice should be classified as an approve named lab policy implementation, not an approve thesis-cited rule implementation. I would not call it an architectural-deviation repair: CarrierLab's global samples/triangles plus duplicated plate-local topology matches the paper's broad structure closely enough for the boundary-shared cases to be legitimate. The deviation to document is epistemic, not geometric: the thesis and CGF paper leave the same-distance, multi-valid boundary-hit winner rule unspecified, so any resolver must be named as lab policy and reported as such.

Summary for independent review: At `2684d95`, CarrierLab has a global triangulation scaffold (`Samples` + `Triangles`) and copied per-plate local triangulations (`Vertices` + `LocalTriangles`) with source backpointers, matching the thesis/CGF global-TDS-to-plate-local architecture. The thesis page images explicitly support global TDS reuse and per-plate duplication/re-indexing, and §3.3.2.3 filters subducting/colliding intersections before assigning crust data from a valid intersected plate. What the sources do not provide is a tie-break for multiple valid same-distance plate hits at shared boundary edges or vertices. The 5,845 IIIE.6.2 holds should therefore be treated as legitimate boundary ambiguities requiring an approved, disclosed lab policy, not as a thesis-cited rule or a structural CarrierLab bug.
