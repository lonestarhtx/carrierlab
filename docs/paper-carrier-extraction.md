# Paper Carrier Extraction

Status: revised pre-Stage-0 deliverable. Stage 0 may not be accepted until the
code audit in `docs/carrier-design.md` is satisfied and the readback is
approved.

Resampling note: the focused reread in
`docs/paper-resampling-extraction.md` is now the canonical reference for
thesis §3.3.2.3 remeshing, multi-hit filtering, q1/q2/qGamma provenance, and
Stage 1.5 reframing. Where this older extraction is less specific about
resampling, the focused doc supersedes it.

## Scope And Source Handling

CarrierLab is now framed as paper-faithful reproduction with Aurous differential
diagnosis. The paper and thesis are treated as the gold standard. The lab is
not trying to prove that the paper is wrong; it is trying to implement the
carrier layer faithfully enough that the diff against Aurous's failed prototype
identifies Aurous's implementation gap.

Sources inspected:

- Cortial et al., "Procedural Tectonic Planets" (2019), extracted to
  `Intermediate/CarrierLabResearchExtracts/ProceduralTectonicPlanets.layout.txt`
- Cortial thesis (2020), extracted to
  `Intermediate/CarrierLabResearchExtracts/Cortial-thesis.layout.txt`
- Driftworld tectonics paper, extracted to
  `Intermediate/CarrierLabResearchExtracts/driftworld-tectonics.layout.txt`
- Aurous failure/reset docs, used only as comparison and cautionary context

Copyright note: the original task asked for every relevant sentence to be
quoted. This document uses short quote anchors and paraphrases the operative
specification so the lab remains reviewable without reproducing long passages.

## Settled Carrier Specification

| Topic | Source locus | Short anchor | Confidence | CarrierLab interpretation |
| --- | --- | --- | --- | --- |
| Planet state | Paper sec. 3, line 213 | "set of tectonic plates" | clear | The planet is modeled as plates on a reference sphere. Plate domains, not a fixed output texture, are the tectonic objects. |
| Crust data sampling | Paper sec. 3, line 269 | "barycentric interpolation" | clear | Crustal/tectonic fields live on sampled triangulations and are evaluated through barycentric interpolation. |
| Moving authority | Paper sec. 3, line 271 | "plates are moving" | clear | Material must move with plate-local geometry. Persistent global sample ownership is not carrier authority. |
| Detach/attach material | Paper sec. 4.2, line 405 | "detach the samples" | clear | The paper treats crust samples as material that can move between plate carriers during events. |
| Global mesh seed | Paper sec. 6, lines 508-510 | "global Spherical Delaunay" | clear | The initial numerical substrate is Fibonacci samples plus a global spherical Delaunay triangulation. |
| Initial partition | Paper sec. 6, line 510 | "initial plates" | clear | Initial plates are built by partitioning the global TDS into plate domains. |
| Resampling cadence | Paper sec. 6, line 517 | "10-60 iterations" | clear | The paper does not remesh every step; it lets plates drift, then periodically resamples. |
| Table 2 baseline | Paper sec. 7.4, lines 639-647 | "1.24" and "40 plates" | clear | The 250k full-pipeline per-step baseline is 1.24s with 40 plates and 0.3 land coverage. CarrierLab kernel time should be faster because it omits later tectonic processes. |
| Plate-local topology | Thesis sec. 3.2.4, lines 2802-2808 | "dupliquer, de re-indicer" | clear | Each plate owns a duplicated, re-indexed, compacted triangulation copied from the global TDS, with empty neighborhood at plate borders. |
| Time step | Thesis sec. 3.3, line 2830 | "2 Ma" | clear | The default tectonic timestep is 2 million years. |
| Geodetic motion | Thesis sec. 3.2.2, line 2582 | "s(p) = omega w x p" | clear | Plate motion is an analytic rotation about an axis through the planet center; Stage 1 drift oracles derive from this. |
| Velocity scale | Thesis sec. 3.2.2, line 2583 | "100 mm" | clear | 100 mm/year is the maximum observed plate-speed scale used by the cadence formula. |
| Per-step interaction geometry | Thesis sec. 3.3.1.3, lines 3138-3140 | "BVH" | clear | Per-plate BVHs are built each step for intersection work. CarrierLab can start brute-force in Stage 0 but must preserve the ray-triangle semantics. |
| Ray query | Thesis sec. 3.3.1.3, lines 3161-3163 | "rayon connectant le centre" | clear | Intersection queries are rays from the planet center toward a target, tested against plate triangles. |
| Resampling period | Thesis sec. 3.3.2.3, lines 3453-3455 | "m = 32 Ma" | clear | Resampling period is `DeltaT = (1-alpha)M + alpha m`, with `alpha = min(1, vm/v0)`, `M=128 Ma`, `m=32 Ma`. |
| Resampling query | Thesis sec. 3.3.2.3, lines 3469-3474 | "rayon-triangle" | clear | For each global TDS vertex, cast the center-to-point ray through every existing plate mesh and barycentrically transfer data from the intersected triangle. |
| Divergent gaps | Thesis sec. 3.3.2.3, lines 3476-3486 | "q1" / "q2" / "qGamma" | clear | Zero valid intersections are divergent gaps. The thesis fills them using nearest boundary points from two different plates and a spherical midpoint ridge estimate. |
| Plate rebuild after resampling | Thesis sec. 3.3.2.3, lines 3492-3508 | "duplication/re-indicage" | clear | After resampling, global TDS triangles are partitioned by assigned plate ids and duplicated/re-indexed into new plate-local triangulations. |
| Render projection | Thesis sec. 4, lines 4123-4125 | "unique triangle" | clear | Downstream render projection also uses a center-to-point ray against model triangles. This confirms the query family, but render projection is not carrier authority. |

## Authoritative Interpretation

1. Plate-local duplicated triangulations are authoritative. The global TDS is
   an initialization substrate, a resampling target, and a diagnostic
   projection surface.

2. Stage 0 must project through ray-from-planet-center triangle intersection,
   not by reading a stored sample owner and not by spherical containment alone.

3. Rigid motion is first-class. The thesis physically updates plate geometry
   each timestep; Driftworld's quaternion accumulator is a documented
   divergence and is not the CarrierLab default.

4. Global resampling is a named cross-window operation. It is not an ownership
   repair heuristic. The resampling event transfers material from current
   plate-local geometry back to the global TDS, then rebuilds plate-local
   triangulations.

5. Gap handling is mostly settled by the thesis. A zero-hit global sample at
   resampling time is a divergent gap filled through the q1/q2/ridge-midpoint
   oceanic-crust algorithm. CarrierLab still records numeric and topology
   miss classes so implementation bugs cannot hide inside this physical rule.
   Current CarrierLab q1/q2 lookup uses a discrete boundary index built from
   boundary-edge endpoints and spherical midpoints; this is a lab approximation
   until exact nearest-boundary provenance is implemented or proven equivalent.

6. Multi-hit handling is only fully specified when subduction/collision has
   already selected losing triangles. In the paper-faithful remesh path,
   subducting/colliding triangles are filtered before candidate selection. For
   no-subduction Stage 1 and standalone Stage 1.5, CarrierLab must use an
   explicit temporary tie-break and report raw multi-hit counts before
   resolving; those policies are diagnostics, not paper-faithful substitutes.

## Remaining Ambiguity

The only open carrier ambiguity for the early lab is the no-subduction
multi-hit tie-break. The approved diagnostic default is nearest plate centroid,
with lowest plate id as a deterministic tie-break, and with raw multi-hit
counts reported separately from resolved output. This ambiguity should not
survive into the primary IIIE remesh path once IIIB/IIIC/IIID process state is
available.

Boundary degeneracy is no longer treated as a paper ambiguity. It is a lab
measurement policy: exact edge/vertex cases are logged as zero-area
degeneracy, resolved by a deterministic half-open rule, and kept visible in
metrics.
