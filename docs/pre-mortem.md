# Carrier Lab Pre-Mortem

Status: revised pre-Stage-0 deliverable.

Assumption: thirty days from now, CarrierLab has failed to produce a
trustworthy reproduction/differential-diagnosis result. This pre-mortem ranks
likely failure modes by likelihood times blast radius and lists evidence that
would distinguish them.

## Evidence Anchors

- Paper extraction: `docs/paper-carrier-extraction.md`
- Driftworld comparison: `docs/driftworld-carrier-comparison.md`
- Design note: `docs/carrier-design.md`
- Aurous failure memo:
  `C:\Users\Michael\Documents\Unreal Projects\Aurous\docs\tectonic-architecture-failure-memo-2026-04.md`
- Aurous reset doc:
  `C:\Users\Michael\Documents\Unreal Projects\Aurous\docs\tectonic-architecture-reset-plate-authoritative-prototype.md`
- Aurous old remesh function:
  `C:\Users\Michael\Documents\Unreal Projects\Aurous\Source\Aurous\Private\TectonicPlanetV6.cpp:18658`
- Aurous Prototype C projection function:
  `C:\Users\Michael\Documents\Unreal Projects\Aurous\Source\Aurous\Private\TectonicPlanetSidecar.cpp:2551`

## Ranked Failure Modes

### 1. Ray-vs-Containment Category Error

Likelihood: high. Blast radius: very high.

The thesis specifies ray-from-planet-center triangle queries for resampling.
If CarrierLab accidentally uses spherical-triangle containment or sample
incident ownership as the operational query, it may reproduce Aurous's failure
class while looking mathematically reasonable.

Evidence:

- code path never constructs the center-to-sample ray
- hit classification depends on stored sample owner or incident triangle list
- ray-triangle and spherical-containment probes disagree near plate borders
- Stage 0 metrics change after replacing containment with ray queries

Investigation checkpoint:

- audit against thesis sec. 3.3.2.3 step 2 and sec. 3.3.1.3
- patch the implementation to ray-triangle if divergence is found
- document provisional-vs-corrected metric diff

### 2. Boundary Degeneracy Looks Like Carrier Failure

Likelihood: high. Blast radius: high.

Global samples may lie exactly on vertices or edges of plate-local triangles.
A raw ray query can find multiple adjacent triangles or plates for a zero-area
condition, making Stage 0 appear overlapped even when area coverage is exact.

Evidence:

- all multi-hit samples have near-zero barycentric weight on at least one edge
- face-centroid probes pass while vertex probes report degeneracy
- half-open resolver gives one resolved owner per sample
- no area-weighted overlap exists

Investigation checkpoint:

- classify boundary-degenerate separately
- fail Stage 0 only for non-degenerate misses or overlaps

### 3. Provisional Stage 0 Scaffolding Is Mistaken For Passing Code

Likelihood: high. Blast radius: high.

The current Stage 0 scaffold was written before the thesis audit and used
sample/incident-triangle projection support. If retained, it would let
diagnostics agree with initialization rather than prove carrier projection.

Evidence:

- projection adds `Sample.PlateId` directly as a hit
- no per-plate local mesh is queried
- raw hit count is forced by initialization rather than geometry
- deterministic hash remains stable even if ray query would fail

Investigation checkpoint:

- replace the scaffold query with ray-from-origin plate-triangle intersection
- include current-code-vs-corrected-code diff in `docs/readback.md`

### 4. Global Resampling Erodes Material

Likelihood: medium-high. Blast radius: very high.

Repeated barycentric/category transfer can erode continental material even when
coverage metrics look acceptable.

Evidence:

- authoritative plate-local CAF remains stable
- projected CAF drops immediately after resampling events
- miss/multi counts are too small to explain loss
- ocean-only and all-continental controls pass or fail asymmetrically

Investigation checkpoint:

- audit barycentric normalization and categorical transfer rule
- compare continuous continental fraction against categorical class transfer
- do not tune gates to hide CAF loss

### 5. Rigid Motion Creates Real Gaps And Overlaps

Likelihood: high. Blast radius: high.

Independent rigid plates naturally open divergent gaps and convergent overlaps
between resampling events. A rigid-only stage gate that demands closed coverage
after motion would contradict the carrier model.

Evidence:

- misses appear only on separating fronts
- overlaps appear only on converging fronts
- gap/overlap area scales with speed and time since last resampling
- zero-motion and single-plate controls stay exact

Investigation checkpoint:

- classify as divergent-gap or convergent-overlap before resolving
- revise only if thesis evidence shows the gate was wrong

### 6. Cumulative Floating-Point Error From Per-Step Vertex Rotation

Likelihood: medium. Blast radius: high.

The thesis updates plate vertices each step. Over 16-64 steps between
resampling events, cumulative floating-point error may perturb unit length,
edge geometry, or barycentric results.

Evidence:

- max/mean vertex displacement from analytic rotation grows each step
- unit-length error grows before resampling
- reprojecting from analytic transform instead of mutated vertices changes hit
  counts
- 60k passes but 250k border queries become unstable

Investigation checkpoint:

- track max/mean analytic-vs-mutated vertex displacement per window
- report unit-length normalization drift separately from projection failures

### 7. BVH Precision Degeneracy At 250k

Likelihood: medium. Blast radius: high.

At 250k samples across 40 plates and 1000 steps, ray-triangle edge cases may
produce false misses or false overlaps, especially when later replaced with a
BVH.

Evidence:

- brute-force and BVH queries disagree on the same geometry
- failures cluster near grazing rays or thin triangles
- small epsilon changes flip many classifications
- high precision or exact-predicate debug path changes results

Investigation checkpoint:

- keep brute-force oracle for small/medium samples
- log numeric-miss and numeric-overlap separately from topology failures
- measure BVH false positive/false negative rates before accepting Stage 3

### 8. Clean-Room Code Recreates Aurous Global Ownership Transport

Likelihood: medium. Blast radius: very high.

The lab could accidentally let global sample ownership carry material,
reintroducing the Aurous contradiction of either anchored material or noisy
reclassification.

Evidence:

- material motion reads previous global sample owner as authority
- recovery, retention, hysteresis, or backfill path affects carrier state
- drift improves only as ownership churn increases
- continents stay visibly anchored while plate geometry rotates

Investigation checkpoint:

- stop implementation work
- remove the authority path
- document the clean-room violation before proceeding

### 9. Numeric Query Failure Reproduces Aurous Signature

Likelihood: medium. Blast radius: high.

A query bug could reproduce Aurous's high miss plus high multi-hit plus CAF
collapse profile.

Aurous comparison anchors from the failure memo:

- 250k step 100: 82,186 misses and 61,187 multi-hits
- 250k step 400: 89,249 misses and 65,779 multi-hits
- CAF collapse from about 0.2980 to 0.0006 over steps 0/100/200/400

Evidence:

- miss/multi rates rise sharply with resolution
- same geometry produces tolerance-sensitive candidate counts
- raw query failures explain projected CAF loss
- Driftworld-like barycentric normalization bug appears after 20-30 steps

Investigation checkpoint:

- audit query and interpolation against thesis and Driftworld sec. 5.6
- compare brute-force and accelerated query paths

### 10. Diagnostic Self-Agreement Masks The Bug

Likelihood: medium. Blast radius: high.

Tests can pass by reading a value produced by the implementation as its own
oracle.

Evidence:

- expected owner equals projected owner because both are read from the same row
- metric and image layers disagree
- negative controls do not fail when intentionally perturbed
- independent analytic drift oracle is absent

Investigation checkpoint:

- require independent recomputation for drift, mass, coverage, and determinism

### 11. Third-Plate Intrusion Creates A Projection State Outside Existing Classes

Likelihood: medium. Blast radius: high.

A sample near an interface may involve two expected neighboring plates plus a
third intruding plate, producing a state that is not a simple miss, overlap,
boundary degeneracy, or two-plate divergent/convergent case.

Detection criteria:

- raw candidate set contains three or more distinct plate ids
- nearest-front classification references a plate pair excluding the resolved
  owner
- local one-ring projected neighbors contain more than two plate ids around a
  sample classified as a two-plate interface
- gap fill would use q1/q2 from plates A/B while the closest valid hit or
  second-best hit is plate C
- the same sample changes pair classification under stable zero-motion replay

Required report fields:

- `third_plate_intrusion_count`
- `third_plate_intrusion_fraction`
- top affected plate triples by sample count
- whether each intrusion is point/edge degeneracy, true area intersection, or
  numeric tolerance artifact

Investigation checkpoint:

- if true area intrusion appears in Stage 0, pause Stage 0 advancement
- if it appears only under motion, add an explicit fifth projection class
  before resampling consumes the sample

### 12. Comparison To Aurous Becomes Apples-To-Oranges

Likelihood: medium. Blast radius: medium-high.

The lab could claim differential diagnosis without matching resolution, seed,
plate count, timestep, metric definitions, or run id.

Evidence:

- comparison rows lack `parameters_match`
- missing baselines are silently omitted
- CAF or miss definitions differ without explanation
- Aurous values are quoted without a run id

Investigation checkpoint:

- mark `no baseline available`
- name the exact Aurous run needed

### 13. Runtime Or Memory Becomes The Finding

Likelihood: medium. Blast radius: medium.

The carrier may be mechanically faithful but too slow or memory-heavy in this
environment.

Evidence:

- `step_kernel_ms` exceeds paper Table 2 at matching resolution
- diagnostic/export cost, not kernel cost, dominates total runtime
- BVH build/query dominates per-step time
- 60k succeeds but 250k or 500k exceeds budget

Investigation checkpoint:

- separate kernel and diagnostic time
- report as performance/quality divergence if spec audit finds no code bug

## Negative Controls

Required before Stage 1 claims:

- zero-motion multi-step run: no drift, no mass change, stable hash
- single-plate run: no misses, no overlaps, exact analytic identity under
  rotation
- no-material ocean-only run: CAF remains zero
- all-continental run: CAF remains one unless named mutation changes it
- two plates moving directly toward each other: forced convergence stress test
- two plates moving directly apart: forced divergence stress test
- one-sample-wide continental terrane: single-sample-feature fragility test
- deliberately perturbed boundary query: should trip coverage diagnostics
- same seed replay: identical metrics and image hashes

## Stop-Condition Doctrine

A stop condition pauses stage advancement and triggers an investigation
checkpoint. The first hypothesis is implementation divergence from the thesis.
Only after a thesis-spec audit fails to find divergence does the result become
a verdict-level finding.

No stage advances silently. No gate is tuned after seeing failure unless the
gate revision is documented with paper/thesis evidence and explicitly approved.
