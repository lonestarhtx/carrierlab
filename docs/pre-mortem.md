# Carrier Lab Pre-Mortem

Status: pre-coding deliverable. No production code is approved by this document.

Assumption: thirty days from now, CarrierLab has failed to produce a trustworthy
verdict. This pre-mortem ranks the likely failure modes by likelihood times
blast radius and lists the evidence that would distinguish each one.

## Evidence Anchors

- Paper extraction: `docs/paper-carrier-extraction.md`
- Driftworld comparison: `docs/driftworld-carrier-comparison.md`
- Aurous failure memo: `C:\Users\Michael\Documents\Unreal Projects\Aurous\docs\tectonic-architecture-failure-memo-2026-04.md`
- Aurous reset doc: `C:\Users\Michael\Documents\Unreal Projects\Aurous\docs\tectonic-architecture-reset-plate-authoritative-prototype.md`
- Aurous old remesh function: `C:\Users\Michael\Documents\Unreal Projects\Aurous\Source\Aurous\Private\TectonicPlanetV6.cpp:18658`
- Aurous old audit log format: `C:\Users\Michael\Documents\Unreal Projects\Aurous\Source\Aurous\Private\Tests\TectonicPlanetV6Tests.cpp:20635`
- Aurous Prototype C projection function: `C:\Users\Michael\Documents\Unreal Projects\Aurous\Source\Aurous\Private\TectonicPlanetSidecar.cpp:2551`
- Aurous nearest-center ownership: `C:\Users\Michael\Documents\Unreal Projects\Aurous\Source\Aurous\Private\TectonicPlanetSidecar.cpp:2923`
- Aurous generic gap/overlap exporter caveat: `C:\Users\Michael\Documents\Unreal Projects\Aurous\Source\Aurous\Private\TectonicMollweideExporter.cpp:462`

## Ranked Failure Modes

### 1. Boundary Degeneracy Looks Like Carrier Failure

Likelihood: high. Blast radius: high.

The global samples may lie exactly on plate mesh vertices or edges. A naive
point-in-triangle query can count multiple adjacent triangles or plates for a
zero-area boundary condition, making Stage 0 appear to have overlaps even when
coverage is exact.

Evidence that distinguishes it:

- raw multi-hit samples are all on edges/vertices within tolerance
- face-centroid probes pass while vertex probes show degeneracy
- half-open resolver gives exact one-owner coverage
- no area-weighted overlap exists

Stop or fix:

- If non-degenerate area overlap exists at Stage 0, stop and report.
- If only boundary degeneracy exists, document the tie policy before code
  proceeds.

### 2. Global Resampling Erodes Material Even In Clean Room

Likelihood: medium-high. Blast radius: very high.

The paper's periodic global resampling may be inherently lossy for categorical
continental material at these sample counts, especially if repeated barycentric
or nearest-category transfer is used. This would show up as CAF decline without
large numeric misses.

Evidence that distinguishes it:

- authoritative plate-local continental mass remains stable
- projected CAF drifts downward after resampling events
- zero-motion and single-plate negative controls pass
- no-hit and multi-hit counts are too small to explain the mass loss

Stop or fix:

- If this appears, do not tune thresholds. Report "viable-with-modifications"
  only if a different material representation, such as continuous continental
  weight with conservative transfer, is justified by evidence.

### 3. Rigid Motion Naturally Creates Real Gaps And Overlaps

Likelihood: high. Blast radius: high.

Independent rigid plates on a sphere create divergent gaps and convergent
overlaps. If Stage 1 is interpreted as requiring complete coverage after motion
without mutation, the paper carrier may fail a gate that contradicts the model.

Evidence that distinguishes it:

- misses occur only between separating plate fronts
- overlaps occur only at converging fronts
- gap/overlap area scales with angular speed and time since last resampling
- zero-motion control remains exact

Stop or fix:

- Do not hide this by filling misses. The checkpoint must state whether the
  gate was wrong for a rigid-only stage or whether the carrier failed to
  classify expected geometry.

### 4. Clean-Room Code Recreates Aurous's Global Ownership Contradiction

Likelihood: medium. Blast radius: very high.

The lab could accidentally make the projection grid the source of truth, then
rediscover the Aurous contradiction: persistent ownership anchors material,
while aggressive reassignment creates noisy topology.

Evidence that distinguishes it:

- material motion is computed from previous global sample owner
- a "recovery" or "retention" path affects authority
- drift improves only when ownership churn rises
- visuals show samplewise reclassification rather than plate-local material

Stop or fix:

- Stop immediately if this appears. It invalidates the clean-room condition.

### 5. Numeric Query Failure At 250k Produces Aurous-Like Miss/Multi Signature

Likelihood: medium. Blast radius: high.

Spherical containment, barycentric interpolation, BVH traversal, or tolerance
handling may fail at 250k and produce the high miss plus high multi-hit pattern
seen in Aurous's paper-like remesh audit.

Aurous comparison anchor:

- the failure memo reports 250k step 100 as 82,186 misses and 61,187 multi-hits
- it reports 250k step 400 as 89,249 misses and 65,779 multi-hits
- it reports CAF collapse from 0.2980 to 0.0006 over steps 0/100/200/400

Evidence that distinguishes it:

- miss/multi rates rise sharply with resolution
- repeated queries of the same geometry are nondeterministic or tolerance
  sensitive
- face-centroid queries differ materially from sample-vertex queries
- high precision or exact predicates change the result

Stop or fix:

- If the same signature appears by Stage 1 step 100, stop and produce a
  verdict report instead of patching around it.

### 6. Diagnostic Self-Agreement Masks The Bug

Likelihood: medium. Blast radius: high.

Tests could pass by reading the same projected value as both implementation and
oracle. Aurous had exporter paths where generic gap/overlap masks were not
evidence of actual sidecar semantics.

Evidence that distinguishes it:

- a test's expected value is copied from the projection result
- image masks disagree with raw metric rows
- independent recomputation is absent
- negative controls do not fail when intentionally perturbed

Stop or fix:

- Require independent recomputation for drift, mass, coverage, and determinism
  before accepting any stage.

### 7. Determinism Breaks Under Parallelism Or Floating Tie-Breaks

Likelihood: medium. Blast radius: high.

At 250k, the implementation may use parallel loops or unordered reductions.
Without deterministic tie-breaks and sorted reductions, same-seed output hashes
may differ.

Evidence that distinguishes it:

- same seed produces different metrics or images
- differences localize to equal-distance plate ties, candidate ordering, or
  parallel reductions
- single-thread mode stabilizes output

Stop or fix:

- Determinism break is a stop condition. Document the failure before changing
  execution order.

### 8. Comparison To Aurous Becomes Apples-To-Oranges

Likelihood: medium. Blast radius: medium-high.

The lab could report "better than Aurous" without matching resolution, step,
seed, plate count, time step, or metric semantics.

Evidence that distinguishes it:

- comparison rows lack baseline values or mark them unclearly
- sample count and step count do not match
- miss/multi definitions differ without explanation
- CAF is computed from a different material definition

Stop or fix:

- Mark "no baseline available" rather than inventing a comparison. Record the
  exact missing run needed.

### 9. Runtime Or Memory Becomes The Verdict

Likelihood: medium. Blast radius: medium.

The carrier might be conceptually viable but impractical at 250k if geometry
queries are too slow or memory-heavy.

Evidence that distinguishes it:

- runtime grows worse than expected with samples or plates
- BVH build/query dominates step time
- memory snapshots exceed the agreed ceiling
- 60k succeeds but 250k cannot run within practical bounds

Stop or fix:

- If practical bounds are exceeded, stop and report "viable-with-modifications"
  or "not viable for the target envelope" depending on the measured bottleneck.

### 10. Visual Coherence Is Judged Without Numbers

Likelihood: medium. Blast radius: medium.

The lab could produce attractive moving continents while quietly losing mass,
creating overlaps, or hiding misses in exports.

Evidence that distinguishes it:

- maps look coherent but metrics show material mass error
- contact sheets omit miss/overlap layers
- image hashes change without metric changes
- no raw JSONL/CSV row exists for the exported images

Stop or fix:

- Report numbers first, images second, verdict last.

### 11. Third-Plate Intrusion Creates A Fifth Projection State

Likelihood: medium. Blast radius: high.

The initial four projection classes may be too small if a global sample sits
near an interface where two expected neighboring plates define the local
divergent or convergent relationship, but a third plate also projects into the
same neighborhood. This can produce a state that is not cleanly an ordinary
miss, overlap, boundary degeneracy, or two-plate divergent/convergent case.

Detection criteria:

- raw candidate set contains three or more distinct plate ids
- nearest-front classification references a plate pair that does not include
  the resolved owner
- local one-ring projected neighbors contain more than two plate ids around a
  sample classified as a simple two-plate interface
- gap fill would use nearest boundary points from plates A/B while the closest
  valid hit or second-best hit is plate C
- the same sample changes pair classification under stable zero-motion replay

Required report fields:

- `third_plate_intrusion_count`
- `third_plate_intrusion_fraction`
- top affected plate triples by sample count
- whether each intrusion is point/edge degeneracy, true area intersection, or
  numeric tolerance artifact

Stop or fix:

- If third-plate intrusion appears in Stage 0 as true area intersection, stop
  and report topology failure.
- If it appears only under motion, add a fifth explicit projection class before
  any resampling or fill policy consumes the sample.

## Negative Controls

Required before Stage 1 claims:

- zero-motion multi-step run: no drift, no mass change, stable hash
- single-plate run: no misses, no overlaps, exact analytic identity under
  rotation
- no-material ocean-only run: CAF remains zero
- all-continental run: CAF remains one unless a named mutation changes it
- deliberately perturbed boundary query: should trip the coverage diagnostic
- same seed replay: identical metrics and image hashes

## Go/No-Go Doctrine

- Stage 0 non-degenerate miss/overlap: no-go, verdict checkpoint.
- Stage 1 Aurous-like high miss plus high multi-hit by step 100: no-go,
  verdict checkpoint.
- CAF collapse without a named material mutation: no-go, verdict checkpoint.
- Determinism break: no-go, verdict checkpoint.
- Any global-sample ownership transport path: invalid clean-room experiment.
