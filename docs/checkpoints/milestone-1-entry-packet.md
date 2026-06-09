# CarrierLab Milestone 1 Entry Packet

Date: 2026-06-09

Branch: `codex/v2-0-carrier`

Base commit: `4799609303e07f8313744454b27de4f31fa9b0a1`

Status: entry packet prepared; implementation requires explicit user approval.

## Decision Context

Milestone 0 was accepted by the user on 2026-06-09 after the verified closeout
report in `docs/checkpoints/milestone-0-closeout-report.md`.

Milestone 1 is the next allowed work. It is not a continuation of the old
historical stage ladder. It is a focused motion checkpoint.

Question: can plate-local authority move over time while projection remains
coherent, deterministic, measurable, and free of hidden repair?

Recommended entry verdict: `READY_FOR_USER_GO_NO_GO_TO_IMPLEMENT_MILESTONE_1`.

## Source Anchors

| Anchor | Local source | Milestone 1 use |
|---|---|---|
| Plate-local authority moves | `docs/paper-carrier-extraction.md` | Moving plate-local triangulations and material remain authority. |
| Geodetic motion | `docs/paper-carrier-extraction.md` | Motion is an analytic rotation about a planet-center axis. |
| Center-ray projection | `docs/paper-carrier-extraction.md` | Projection queries cast from the planet center through global substrate samples. |
| Performance baseline | `docs/paper-carrier-extraction.md` and `docs/carrier-design.md` | 250k/40-plate paper full-pipeline baseline is about 1.24s per step; CarrierLab motion-only kernel should be faster. |
| Rigid motion creates real gaps and overlaps | `docs/pre-mortem.md` | Misses and overlaps are expected evidence, not failures by themselves. |
| Global samples are not authority | `docs/carrier-design.md` | Projection output cannot repair or mutate plate-local material. |

## External Review Integration

The Fable review is accepted as useful design-review input, not as authority.
Milestone 1 adopts these points:

- replace the historical source-adjacent scale query shortcut with a real
  moved-surface query path;
- use static per-plate AABB trees in plate-local coordinates and inverse-transform
  rays into each plate frame;
- split the old single `RayEpsilon` into named tolerances before setting drift
  and projection gates;
- keep report ceremony light and put enforcement in counters, predicates,
  replay hashes, and independent oracles.

Milestone 1 parks these points for later milestones:

- repeated resample-diffusion and field-sharpness gates belong to Milestone 2;
- contact-local material polarity belongs to Milestone 3;
- persistent ridge identity belongs before terrain/elevation amplification,
  not before rigid-motion projection.

## Allowed Scope

Milestone 1 may add or revise:

- rigid plate motion represented by analytic rotation transforms;
- per-plate static `FDynamicMeshAABBTree3` query structures over plate-local
  triangulations;
- inverse-ray projection from fixed global substrate samples into moved
  plate-local authority;
- brute-force micro-oracle comparison for accelerated query correctness;
- split tolerance configuration and tolerance metrics;
- analytic motion oracle over vertices and material-vector fields that exist at
  this milestone;
- raw miss, raw overlap, boundary-degenerate, and third-plate readout classes;
- barycentric material readout for projection diagnostics;
- authority hash, projection hash, replay hash, and performance metrics;
- a single Milestone 1 closeout report.

## Forbidden Scope

Milestone 1 must not add:

- remeshing, resampling, q1/q2 gap fill, or qGamma material generation;
- subduction, collision, rifting, uplift, erosion, slab pull, or terrain;
- contact polarity decisions that consume material or topology;
- persistent global sample ownership as material or tectonic authority;
- ownership repair, retention, hysteresis, backfill, anchoring, or prior-owner
  fallback;
- centroid, random, nearest, or prior-owner overlap winner as a paper-faithful
  resolver;
- UI or viewport output as pass/fail evidence.

Raw misses and overlaps may be classified. They may not be repaired.

## Query Architecture

Milestone 1 replaces the historical source-adjacent scale projection shortcut.

The intended query path is:

1. Build one static `FDynamicMesh3` and one static `FDynamicMeshAABBTree3` per
   plate after plate-local topology is created.
2. Store each plate's motion as an accumulated rotation transform from its
   canonical plate-local frame to current world/planet frame.
3. For each global substrate sample `p`, define the center ray from the planet
   origin through `p`.
4. For each candidate plate, inverse-transform the ray by that plate's current
   rotation into the plate-local frame.
5. Query the static per-plate AABB tree in that frame.
6. Convert every hit into a common projection record:
   plate id, local triangle id, source triangle id, barycentrics, ray distance,
   boundary class, material readout, and tolerance flags.
7. Classify the sample from the raw hit set without mutating authority.

Candidate plate selection for the first Milestone 1 implementation should be
all plates. A later angular-cap or broadphase optimization may be added only
after it has an equivalence gate against the all-plate query.

This design keeps the expensive tree static across rigid motion. Motion changes
the ray, not the tree.

## Tolerance Contract

Milestone 1 must retire the single-purpose use of `RayEpsilon` for multiple
geometry decisions.

Required named tolerances:

| Tolerance | Meaning | Gate use |
|---|---|---|
| `ray_parallel_tolerance` | Dimensionless near-parallel threshold for ray/triangle plane tests. | Prevents triangle-area-dependent parallel rejection. |
| `ray_t_min_tolerance` | Minimum acceptable ray distance from the planet center. | Separates invalid origin/behind-ray hits from barycentric misses. |
| `barycentric_slop` | Dimensionless inside-triangle band. | Controls inside/outside classification. |
| `boundary_band` | Dimensionless edge/vertex degeneracy band. | Controls boundary-degenerate reporting. |
| `unit_length_tolerance` | Unit-sphere normalization tolerance. | Motion and projection sanity gate. |
| `motion_oracle_tolerance_km` | Analytic-vs-computed vertex motion tolerance. | Motion oracle gate. |

The closeout report must print all tolerance values. Changing a tolerance after
seeing a failure requires a documented reason and user approval.

## Fixture Plan

Milestone 1 should use tiny fixtures as microscopes and scale fixtures as
performance and liveness gates.

| Fixture | Purpose | Required result |
|---|---|---|
| Zero-motion multi-step | Baseline replay and no-op transform. | Stable hashes, zero material attachment errors, no nondegenerate misses or overlaps. |
| Single-plate rotation | Pure rigid rotation without inter-plate boundaries. | Exact analytic motion, no raw gaps/overlaps, accelerated query matches brute force. |
| Stable seam | Two plates with identical or no relative seam motion. | Boundary-degenerate hits remain boundary-only, no repair. |
| Forced divergence | Two plates separate. | Nonzero raw misses, zero repair, no gap fill. |
| Forced convergence | Two plates overlap. | Nonzero raw overlaps, zero resolver consumption. |
| Third-plate intrusion | A sample/ray neighborhood sees a third plate after motion. | Explicit third-plate class, no silent pair collapse. |
| Perturbed-boundary sliver | Tolerance stress near skinny or near-boundary triangles. | Accelerated and brute-force classifications match, tolerance counters explain edge cases. |

Scale ladder:

| Scale | Role | Required for closeout |
|---|---|---|
| Micro | Correctness microscopes. | Hard gate. |
| 50k, 40 plates | Minimum meaningful paper-scale carrier check. | Hard gate. |
| 250k, 40 plates | Main comparison against paper Table 2 baseline. | Hard gate for serious readiness; performance finding if not within budget. |
| 500k, 40 plates | Target confidence scale. | Stretch gate; if not run, report must say why. |

## Metrics And Gates

Required metric families:

- `plate_aabb_build_ms`
- `inverse_ray_projection_kernel_ms`
- `step_kernel_ms`
- `step_with_diagnostics_ms`
- `peak_memory_mb`
- `global_sample_count`
- `plate_count`
- `tree_triangle_count_sum`
- `ray_query_count`
- `aabb_hit_count_total`
- `bruteforce_hit_count_total` for micro/equivalence fixtures
- `aabb_bruteforce_classification_mismatch_count`
- `raw_motion_miss_count`
- `raw_motion_overlap_count`
- `boundary_degenerate_count`
- `third_plate_intrusion_count`
- `material_attachment_error_count`
- `projection_reads_global_owner_count`
- `motion_repair_count`
- `remesh_during_motion_count`
- `primary_resolver_consumed_count`
- `analytic_motion_max_error_km`
- `analytic_motion_mean_error_km`
- `unit_length_max_error`
- `authority_hash`
- `projection_hash`
- `replay_authority_hash`
- `replay_projection_hash`

Hard pass predicates:

- all micro fixtures pass their independent oracles;
- 50k scale passes correctness gates;
- 250k scale runs and produces a performance comparison unless blocked by an
  approved machine/runtime issue;
- accelerated AABB query matches brute-force classification on all micro and
  equivalence fixtures;
- `projection_reads_global_owner_count == 0`;
- `motion_repair_count == 0`;
- `remesh_during_motion_count == 0`;
- `primary_resolver_consumed_count == 0`;
- material attachment errors are zero;
- replay hashes match for authority and projection outputs;
- raw misses and overlaps are reported before any downstream interpretation;
- performance fields separate kernel time from diagnostics/reporting time.

## Performance Budget

Milestone 1 uses the existing `docs/carrier-design.md` budget table:

| Resolution | Kernel total for 1000 steps | Diagnostics total for 1000 steps | Memory ceiling |
|---|---:|---:|---:|
| 60k | <= 2 minutes | <= 5 minutes | <= 2 GB |
| 100k | <= 5 minutes | <= 15 minutes | <= 4 GB |
| 250k | <= 20 minutes | <= 45 minutes | <= 8 GB |
| 500k | <= 45 minutes | <= 90 minutes | <= 16 GB |

For 250k/40 plates, the paper full-pipeline per-step baseline is about `1.24s`.
Milestone 1 is only rigid motion plus projection and diagnostics, so
`step_kernel_ms` should be faster than the paper full-pipeline baseline. If it
is slower, the closeout report must treat that as an investigation finding, not
as a quiet pass.

## Stop Conditions

Pause Milestone 1 implementation and write an investigation note if any of these
appear:

- accelerated AABB classification disagrees with brute-force on a micro fixture;
- 50k cannot complete without changing carrier authority rules;
- 250k kernel time is slower than the paper full-pipeline baseline without a
  documented cause;
- any implementation path reads previous global sample owner as authority;
- any repair, retention, hysteresis, prior-owner fallback, or UI-driven
  correction affects carrier state;
- raw overlap is consumed by a primary resolver before process state exists;
- tolerance values are tuned after failure without a documented reason;
- replay determinism breaks;
- memory exceeds the budget ceiling;
- report verdicts rely on self-described status rather than explicit predicate
  fields.

## Implementation Order

1. Add the Milestone 1 motion/query config with named tolerances.
2. Add per-plate static tree construction and inverse-ray query records.
3. Add brute-force equivalence checks for micro fixtures.
4. Replace scale projection's source-adjacent shortcut with all-plate inverse-ray
   tree queries.
5. Add metrics, hard predicates, and replay hashing.
6. Add commandlet/automation coverage for the fixture plan.
7. Run micro, 50k, and 250k gates.
8. Optionally run 500k if 250k passes and runtime is acceptable.
9. Write `docs/checkpoints/milestone-1-closeout-report.md`.
10. Stop for explicit user go/no-go before Milestone 2.

## Closeout Report Shape

The Milestone 1 closeout report must include:

- decision question and verdict;
- source anchors and external-review deltas accepted or rejected;
- query architecture summary;
- tolerance table;
- fixture gate table;
- AABB-vs-brute-force equivalence table;
- raw miss/overlap/third-plate classification table;
- forbidden fallback counter table;
- replay hash table;
- performance ladder with kernel and diagnostics split;
- known limits;
- explicit recommendation for or against preparing the Milestone 2 entry packet.

## Entry Verdict

Milestone 1 is ready to implement after explicit user approval.

The implementation must focus on inverse-ray rigid-motion projection. It must
not advance into remesh, process physics, terrain, or editor tooling.
