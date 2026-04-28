# Driftworld Carrier Comparison

Status: revised pre-Stage-0 deliverable. Driftworld is used as independent
confirmation and contrast, not as authority over the Cortial thesis.

## Sources

- Driftworld paper:
  `Intermediate/CarrierLabResearchExtracts/driftworld-tectonics.layout.txt`
- Cortial paper/thesis extraction:
  `docs/paper-carrier-extraction.md`
- Aurous failure memo, for comparison discipline only

## Executive Read

Driftworld independently re-derives the same carrier family: a broken,
plate-separated crust layer carries simulation state, while a compact closed
data layer receives resampled output. Its apparent success supports the thesis
algorithm as implementable, but it is not the lab specification. Where
Driftworld diverges from the thesis, CarrierLab follows the thesis.

The most important confirmation is architectural: simulation authority is not
the render/output layer. Driftworld explicitly separates crust carrier data
from compact output data, which matches the thesis's plate-local triangulation
plus global-resampling structure.

## Layer Comparison

| Topic | Cortial thesis | Driftworld | CarrierLab decision |
| --- | --- | --- | --- |
| Carrier layer | One duplicated spherical triangulation per plate | Crust layer has broken topology at plate borders | Adopt plate-local duplicated carrier meshes. |
| Output layer | Original global TDS is reused for resampling | Data layer is a compact closed sphere mesh | Treat global samples as projection/resampling output, not persistent transport authority. |
| Render layer | Downstream terrain amplification/rendering | Separate Render layer due Unity limits | Out of CarrierLab scope except diagnostic PNG exports. |
| Plate motion | Plate mesh vertices are updated each step | Plate quaternion transform is accumulated and applied when needed | Follow thesis: per-step vertex motion for Stage 1. Track FP drift explicitly. |
| Resampling | Periodic `DeltaT` event, then rebuild plate meshes | Periodic/event-forced resampling onto original mesh | Implement as Stage 1.5, separate from within-window rigid projection. |
| Overlap resolution | Subduction/collision filter removes losing triangles | Rank score can decide precedence | Do not import rank score. Stage 1 uses explicit no-subduction tie-break and raw counts. |

## Driftworld Evidence

Short anchors from Driftworld:

- Sec. 4.2, lines 1427-1431: "Crust keeps all the tectonic model information";
  "Data layer provides compact surface data."
- Sec. 4.2, lines 1429-1432: crust topology is "broken along the plate borders";
  ridges are present in Data because they are used for resampling.
- Sec. 3.7, lines 1112-1117: resampling is used "to fill the gaps" and
  interpolate current broken-surface data onto the original mesh.
- Sec. 3.6, lines 1058-1062: Driftworld updates a quaternion transform by
  `omega dt` and plates move as rigid bodies.
- Sec. 4.2.1, lines 1468-1471: vertex positions "do not change" and transforms
  act on them when needed.
- Sec. 5.1, lines 1968-1971: interpolation artifacts appear along plate
  borders and often have single-sample size.
- Sec. 5.6, lines 2043-2045: past crust artifacts were caused by incorrectly
  normalized barycentric interpolation plus crust resampling.

## What Driftworld Confirms

1. Broken-topology crust authority plus compact closed output is a workable
   architecture. This strengthens the case that Aurous's earlier failure was
   implementation-specific or environment-specific, not an obvious conceptual
   impossibility.

2. Periodic resampling is not optional glue. It is the operation that reconciles
   drifting, broken plate-local crust with a closed global surface.

3. Barycentric interpolation and resampling are load-bearing and fragile. The
   Driftworld past-issue note maps directly to CarrierLab diagnostics: if
   normalization is wrong, artifacts can appear after only 20-30 steps.

4. Rendering/export artifacts can be unrelated to carrier correctness.
   Driftworld's missing texture issue is explicitly a rendering problem. In
   CarrierLab, PNGs can never pass a stage without raw metrics.

## Where CarrierLab Must Not Follow Driftworld

- Do not adopt the quaternion accumulator as the canonical motion model. The
  thesis updates plate geometry per step; CarrierLab follows that and measures
  cumulative floating-point drift.

- Do not adopt whole-plate merge behavior or rank-based overlap precedence.
  Those are Driftworld simplifications around collision/subduction, which are
  outside the early CarrierLab scope.

- Do not use Driftworld defaults as paper baselines. Driftworld's table lists
  20 plates and zero initial continental probability for its model value,
  while the paper Table 2 baseline is 40 plates and 0.3 land coverage.

- Do not treat Driftworld's visual success as a substitute for miss, overlap,
  CAF, mass, drift-coherence, determinism, memory, and step-kernel metrics.

## Implication For Differential Diagnosis

If CarrierLab reproduces the thesis carrier cleanly, the diff against Aurous's
failed prototype becomes the bug list. Driftworld already points to likely
high-value suspects: whether Aurous used ray-from-origin triangle intersection
or a different containment predicate, whether barycentric normalization was
independently audited, whether broken plate-local authority was kept separate
from compact output state, and whether render/export artifacts were mistaken
for carrier facts.
