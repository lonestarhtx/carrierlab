# Phase II Subduction Design

## Status

Draft for user review. No Phase II production code is approved by this document alone.

## Context

Phase I established the carrier foundation through Stage 0, Stage 1, and Stage
1.5. The accepted Stage 1.5 result is that the carrier can be characterized
cleanly, but convergent-zone multi-hit resolution is process-coupled. Centroid,
synthetic-subduction, and random tie-break policies changed material and
per-plate area outcomes. The next natural extension is therefore not a longer
carrier run; it is a subduction prototype that supplies the physical state the
thesis resampling algorithm expects when filtering overlapping triangles.

Phase II remains CarrierLab research code. It is not Aurous 2, not terrain
beauty, and not a game integration slice.

## Decision

Build subduction as a new prototype layer on top of the Phase I carrier. The
subduction layer owns convergent-boundary process state. The carrier continues
to own plate-local crust geometry and resampling mechanics.

The first Phase II implementation must answer one question:

Can explicit, deterministic subduction state resolve convergent multi-hit
projection without the policy-sensitive CAF and per-plate area failures observed
in Stage 1.5?

## Authority Model

- Plate-local crust geometry remains authoritative for material transport.
- Global TDS samples remain projection and resampling targets, not tectonic
  authority.
- Subduction state is keyed by convergent plate-pair boundary evidence, not by
  persistent global sample ownership.
- A triangle is eligible for the thesis resampling filter only if Phase II
  process state marks it subducting or colliding.
- Projection overlays, actor colors, and HUD values are diagnostics only.

## Initial Data Model

Phase II should introduce explicit state types rather than overloading plate ids
or sample labels:

- `FCarrierSubductionContact`: plate pair, boundary locus, polarity, confidence,
  signed convergence velocity, and step range.
- `FCarrierTriangleProcessLabel`: plate id, local triangle id, source triangle
  id, process label, and label reason.
- `FCarrierSubductionMetrics`: raw convergent overlaps, labeled overlaps,
  filtered triangles, unresolved overlaps, CAF before/after, per-plate area
  deltas, determinism hash.

The exact C++ names may change, but the separation must not: contact detection,
triangle labeling, and resampling filtering are separate surfaces.

## Non-Goals

- No collision/orogeny terrain output.
- No rifting, uplift, erosion, slab pull, or rendering polish.
- No global sample ownership persistence.
- No repair, retention, hysteresis, or hidden anchoring heuristic.
- No Aurous V6/V9/Prototype C/D/E port.

## Required Gates

- Same-seed replay produces byte-identical subduction labels and event logs.
- Forced convergence produces labeled subduction contacts with unresolved
  non-boundary multi-hit rate below 2% immediately after resampling.
- Forced divergence produces no subduction contacts.
- Zero-motion produces no subduction contacts.
- All-continental and ocean-only controls do not create spurious material class
  changes.
- Per-plate area delta remains below the Phase II gate chosen in the slice plan,
  or the report documents why physical subduction legitimately exceeds it.
- Stage 1.5 policy-sensitive metrics are re-run with explicit subduction labels
  replacing centroid/random policy and compared side by side.

## Diagnostics

Phase II reports must include:

- Contact map overlay.
- Subducting-triangle mask.
- Filtered-overlap mask.
- Unresolved-overlap mask.
- Authoritative CAF and projected CAF as separate series.
- Per-plate area delta table.
- Event log with contact id, triangle id, plate pair, polarity, signed velocity,
  and filter decision.

## Consequences

If Phase II succeeds, CarrierLab can distinguish process-independent carrier
behavior from process-supplied convergent resolution. That gives Aurous a clear
integration target: adopt the carrier plus explicit process-state filtering, not
the Stage 1.5 centroid policy.

If Phase II fails, the failure should be localized: contact detection, triangle
labeling, filter application, or material transfer. That is the point of
starting Phase II as a prototype rather than folding it into a long-horizon
carrier run.
