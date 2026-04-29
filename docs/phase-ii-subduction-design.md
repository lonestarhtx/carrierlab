# Phase II Subduction Design

Status: draft for user review. This document is a design contract, not approval
to implement Phase II production code.

## Context

Stage 1.5 characterized the Phase I carrier foundation under periodic
resampling. Coverage can be closed after resampling events, but mixed-material
convergent behavior is sensitive to the multi-hit policy. Centroid,
synthetic-subduction, and random policies changed material and per-plate area
outcomes. That is exactly the surface the Cortial thesis delegates to
subduction/collision process state: overlapping triangles are not resolved by a
generic carrier tie-break; process labels decide which triangles are ignored.

Phase II therefore starts a new prototype layer on top of the validated carrier.
It is still CarrierLab research code, not Aurous 2 and not a terrain feature
slice.

## Decision

Build explicit subduction process state as a separate layer that feeds the
existing resampling filter hook.

The carrier continues to own:

- plate-local duplicated triangulations
- plate-local material fields
- rigid motion and resampling mechanics
- projection diagnostics

The subduction layer owns:

- convergent contact detection
- over/under or ambiguous polarity state
- plate-local triangle process labels
- filter decisions consumed by resampling
- event logs and process-state hashes

The primary Phase II question is:

Can explicit deterministic subduction labels reduce policy-sensitive
convergent multi-hit behavior without introducing CAF loss, broad boundary
area-fill, anchored material, or global-sample authority?

## Authority Rules

- A global sample id may appear in evidence logs, but it cannot be the source of
  persistent process authority.
- A contact is keyed by plate pair, step/window, boundary evidence, and local
  plate-triangle references.
- A triangle is filtered only when a `Subducting` triangle-process label exists.
- `Ambiguous`, `CollisionCandidate`, and `ThirdPlateIntrusion` are reportable
  states, not silent fallbacks.
- Centroid and random policies remain comparison controls and are never the
  primary Phase II resolution path.

## Initial Data Model

Names may evolve in code, but the separations are binding.

`FCarrierSubductionContact`

- stable contact id
- ordered plate pair
- boundary evidence point on the unit sphere
- signed convergence velocity
- polarity state: over plate, under plate, ambiguous, or fixture-specified
- confidence/margin
- source step and event window
- third-plate intrusion flag

`FCarrierTriangleProcessLabel`

- contact id
- plate id
- plate-local triangle id
- source global triangle id
- label: none, subducting, overriding, collision-candidate, ambiguous
- label reason
- distance from contact evidence
- signed velocity margin

`FCarrierSubductionEvent`

- event id and step
- policy/config id
- contact ids considered
- triangle labels emitted
- triangles filtered during resampling
- unresolved multi-hit count
- material/accounting deltas
- hash inputs in canonical order

`FCarrierSubductionMetrics`

- raw overlaps
- labeled overlaps
- filtered triangles
- unresolved overlaps after filtering
- third-plate contacts
- ambiguous polarity count
- authoritative CAF and projected CAF
- per-plate area and material deltas
- timing slices
- process-state hash and projection hash

## Contact Detection

Contact detection is geometry-derived and recomputed from current plate-local
state.

Inputs:

- raw multi-hit candidates from ray-from-origin projection
- plate-local triangle ids and barycentric weights
- current plate motion vectors
- local boundary evidence from the participating plate meshes

Classification:

- convergent contact: signed relative motion indicates the two plate fronts move
  toward each other at the evidence point
- divergent contact: separating motion; never subduction
- transform/low-margin contact: signed velocity near zero; report as ambiguous
- third-plate contact: three or more non-boundary plates participate; report
  separately

The signed velocity calculation must be tested with mirrored forced-convergence
and forced-divergence fixtures. Magnitude-only or absolute-dot classification is
a stop condition.

## Polarity Policy

Phase II does not yet have the full paper material stack: no oceanic age, ridge
direction, density proxy, collision model, or terrain response. The first
polarity policy is therefore conservative:

- If one side is predominantly oceanic and the other predominantly continental,
  the oceanic side is the under plate.
- If both sides are oceanic and no age proxy exists, polarity is ambiguous
  unless a fixture supplies it.
- If both sides are continental, classify as collision-candidate or ambiguous;
  do not invent subduction.
- Test fixtures may provide explicit polarity to prove the filter path.

This keeps "subduction label exists" distinct from "carrier picked a convenient
winner." That distinction is the point of Phase II.

## Triangle Labeling

Triangle labels are derived from contacts and are local to plate meshes.

Rules:

- Label only triangles within a bounded distance from contact evidence.
- Record source contact id and reason for every label.
- Filter area must scale with boundary length, not with overlap area.
- Broad masks are a failure unless a later approved physical process explicitly
  creates them.
- No label may be emitted in zero-motion, single-plate, or forced-divergence
  controls.

## Resampling Integration

Phase II integrates at the existing thesis hook:

1. Project each global TDS sample by ray-from-origin triangle queries.
2. For each raw candidate, reject the candidate only if its plate-local triangle
   has a `Subducting` process label active for the current resampling event.
3. If one candidate remains, barycentrically transfer material.
4. If zero candidates remain because all candidates were filtered, report
   `filter-exhausted`; do not silently gap-fill.
5. If zero candidates existed before filtering, use the q1/q2 divergent-gap
   path from Stage 1.5.
6. If multiple candidates remain, report unresolved multi-hit. Centroid/random
   may be run as comparison controls, not as the primary result.

## Gates

Slice gates are refined in `docs/phase-ii-slice-plan.md`; the design-level gates
are:

- Same-seed replay produces byte-identical contacts, labels, event logs, and
  post-event hashes.
- Zero-motion, single-plate, and forced-divergence controls produce no
  subduction contacts or labels.
- Forced-convergence with fixture polarity produces contacts, labels the
  expected under plate, and swaps labels when polarity is swapped.
- Post-filter non-boundary multi-hit rate is below 2% at 60k in the primary
  cadence test, or the report explains the unresolved class.
- Authoritative CAF changes only through named material events.
- Per-plate area deltas reconcile with filter/material event records.
- Third-plate intrusion is never folded into ordinary two-plate subduction.

## Diagnostics

Every Phase II checkpoint must include:

- contact map
- subducting-triangle mask
- filtered-overlap mask
- unresolved-overlap mask
- ambiguous-contact mask
- third-plate-contact mask
- authoritative CAF and projected CAF time series
- per-plate area and material delta table
- event log with contact id, triangle id, plate pair, polarity, signed velocity,
  label reason, and filter decision
- timing split for projection, contact detection, triangle labeling, filtering,
  resampling, hashing, and export

## Non-Goals

- No terrain elevation or rendering beauty.
- No collision/orogeny implementation beyond explicit ambiguous or
  collision-candidate labels.
- No rifting, uplift, erosion, slab pull, or climate/material post-process.
- No global sample ownership persistence.
- No hidden tie-break that replaces process labels.
- No Aurous implementation port.

## Consequences

If Phase II succeeds, CarrierLab will have separated the process-independent
carrier foundation from the process-supplied convergent filter. Aurous's useful
integration target becomes "carrier plus explicit process-state filtering," not
Stage 1.5's centroid policy.

If Phase II fails, the failure should localize to one surface: contact detection,
polarity, triangle labeling, filter integration, material accounting, or
performance. The packet is structured so that those failures are evidence, not
tuning targets.
