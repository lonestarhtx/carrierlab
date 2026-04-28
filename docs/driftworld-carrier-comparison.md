# Driftworld Carrier Comparison

Status: pre-coding deliverable. No production code is approved by this document.

## Sources

- Driftworld paper: `C:\Users\Michael\Documents\Unreal Projects\Aurous\docs\driftworld-tectonics\driftworld-tectonics.pdf`
- Paper extraction: `docs/paper-carrier-extraction.md`
- Aurous failure memo: `C:\Users\Michael\Documents\Unreal Projects\Aurous\docs\tectonic-architecture-failure-memo-2026-04.md`

## Executive Comparison

Driftworld is an independent Unity implementation inspired by Cortial et al.,
but it is not a literal clone. It keeps the central carrier idea: crust data
lives on moving plate geometry and is periodically resampled to a compact
closed surface layer. Its strongest value to CarrierLab is architectural
triangulation: it shows that an implementation can separate the simulation
carrier from the render/output surface and still produce plausible results.

It also documents deviations that matter for falsification. Driftworld merges
whole plates on continental collision, omits fold direction, uses simplified
ridge placement, uses a ranking score for overlap precedence, and relies on
Unity float/compute-shader infrastructure. Those choices mean Driftworld's
success cannot prove the paper exactly, but it strongly informs the lab's
design and diagnostics.

## Layer Model

Driftworld has three layers:

- Crust layer: the simulation carrier. It has broken topology along plate
  borders and moves by plate transforms.
- Data layer: a compact closed sphere surface used for resampled output and
  overlay data.
- Render layer: a lower-resolution mesh used because of Unity mesh limits.

Short source anchor from Driftworld: "Crust keeps all the tectonic model information";
"Data layer provides compact surface data."

CarrierLab interpretation:

- The paper carrier should be tested as plate-local authority plus downstream
  projection/resampling, not as persistent global sample ownership.
- CarrierLab should name these layers explicitly to prevent authority drift:
  `CarrierPlateMesh`, `ProjectionGrid`, and `ExportSurface`.

## Where Driftworld Matches The Paper

| Topic | Paper | Driftworld | CarrierLab implication |
| --- | --- | --- | --- |
| Unit sphere | Sphere centered at origin | Unit sphere simulation, scaled for rendering | Use unit sphere and report physical km only as derived units. |
| Sampling | Fibonacci samples plus spherical Delaunay | Prepared Delaunay meshes | Use deterministic sample/mesh generation with hashes. |
| Plate identity | Plate domains with plate-local crust fields | Equivalence classes of vertices and plate objects | Store plate-local authority, not global output authority. |
| Motion | Rigid rotation by axis and speed | Quaternion transform per plate | Analytic rotation gives independent drift oracle. |
| Gaps | Divergence creates oceanic crust | Surface voids are filled during resampling | Misses must be classified before fill. |
| Resampling | Periodic global resampling every 10-60 iterations | Resampling interpolates current broken crust onto original mesh | Resampling is an experimental operation, not a hidden cleanup. |
| Acceleration | BVH for plates | BVH and compute shaders | CarrierLab needs an acceleration strategy by Stage 3. |

## Where Driftworld Differs

| Topic | Driftworld difference | Risk for CarrierLab |
| --- | --- | --- |
| Initial parameters | Defaults to 20 plates and, for testing, zero initial continental probability | Cannot use Driftworld defaults for the paper's 40 plate / 0.3 land baseline without documenting the change. |
| Fold direction | Not implemented | Driftworld success does not validate every paper material field. CarrierLab Stage 0-2 can omit fold direction only if the omission is listed as out-of-scope for rigid transport. |
| Collision | Whole plates can merge; paper attaches connected terranes | Do not infer terrane-transfer correctness from Driftworld. CarrierLab stages before mutation should avoid collision features entirely. |
| Overlap precedence | Plate rank score decides "goes under" | Ranking is a heuristic, not paper proof. Lab overlap diagnostics must report raw geometry before resolving. |
| Resampling | Forces resampling on continental collision and resets transforms | This is a design adaptation. CarrierLab should separate periodic paper resampling from event-triggered resampling. |
| Ridge estimate | Uses nearest two plates and midpoint assumptions | Useful as a fallback model, but the lab should make ridge construction auditable. |
| Precision | Mostly float precision, with tolerances and clamping | CarrierLab should use double precision and classify tolerance effects. |
| Rendering | Missing texture data can be a rendering-only issue | Exports cannot be accepted without raw metric backing. |

## What Driftworld's Apparent Success Tells Us

Driftworld weakly supports the hypothesis that Aurous's old failure was not
inevitable. It reports robust enough 500k data output to avoid serious
irrecoverable artifacts, while acknowledging runtime, memory, interpolation,
and missing-texture issues.

That is not enough to answer the CarrierLab question. Driftworld does not
report the required miss/multi-hit/CAF/drift-coherence metrics, and it changes
several core mechanics. It is evidence that the carrier family is plausible,
not evidence that the paper carrier passes at 60k-250k under the proposed
gates.

## Design Lessons Imported Into CarrierLab

- Separate simulation authority from output layers.
- Record plate transforms rather than physically mutating every position each
  step.
- Treat resampling as a named event with before/after metrics.
- Use raw geometry diagnostics before any winner selection.
- Keep texture/export errors separate from carrier errors.
- Include precision and tolerance diagnostics from the beginning.

## Design Lessons Not Imported

- Do not use Unity float precision as precedent.
- Do not use whole-plate merge behavior as paper evidence.
- Do not accept "visually robust" as a substitute for miss, overlap, CAF, and
  drift metrics.
- Do not let render layer constraints determine simulation authority.

