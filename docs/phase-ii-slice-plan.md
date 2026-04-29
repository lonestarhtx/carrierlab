# Phase II Slice Plan

Phase II starts only after the user accepts this plan or edits it. Each slice
ends with a short checkpoint note and an explicit go/no-go.

## Slice 0: Spec Audit And Harness

Goal: turn the design note into executable fixtures without adding subduction
behavior yet.

Work:

- Add a Phase II commandlet or test harness that reuses the Phase I carrier.
- Add two-plate convergence, two-plate divergence, zero-motion, polarity-swap,
  and triple-junction fixtures.
- Add event-log schema for contacts, triangle labels, and filter decisions.
- Add full determinism hashes for contact state and projected output.

Exit gate:

- Fixtures build and run with no subduction labels enabled.
- Stage 1.5 baseline metrics are reproduced in the Phase II harness.

## Slice 1: Contact Detection

Goal: detect convergent plate-pair boundary contacts without affecting
projection or resampling.

Work:

- Compute signed convergence velocity at boundary evidence points.
- Emit `FCarrierSubductionContact` records with plate pair, polarity candidate,
  confidence, velocity margin, and source evidence.
- Keep ambiguous and third-plate cases as explicit diagnostic classes.

Exit gate:

- Forced convergence produces contacts.
- Forced divergence and zero-motion produce none.
- Polarity-swap fixture swaps the candidate over/under plate.
- Contact log replay is byte-identical for same seed.

## Slice 2: Triangle Labeling

Goal: map contact records to plate-local triangles with auditable reasons.

Work:

- Label local triangles near convergent contacts as subducting candidates.
- Record source triangle id, local triangle id, plate id, contact id, and
  distance/margin evidence.
- Add overlays: contact map, subducting-triangle mask, ambiguous-contact mask.

Exit gate:

- Every filtered candidate triangle is traceable to a contact.
- Filtered area is bounded relative to boundary length.
- No labels appear in zero-motion, divergence, or single-plate controls.

## Slice 3: Resampling Filter Integration

Goal: replace Stage 1.5 convergent multi-hit lab policy with explicit triangle
filtering.

Work:

- Wire triangle process labels into the thesis resampling filter hook.
- Re-run the Stage 1.5 cadence-faithful 60k case with explicit labels.
- Report unresolved multi-hits separately from filtered overlaps.
- Keep centroid/random policies only as comparison controls, not as the primary
  path.

Exit gate:

- Immediately after resampling, miss rate and non-boundary multi-hit rate are
  below 2% at 60k.
- Authoritative CAF is stable unless a named subduction material mutation has
  been explicitly enabled.
- Per-plate area deltas are below the agreed gate or explained by labeled
  physical subduction.

## Slice 4: Material Accounting

Goal: make subduction effects explicit rather than accidental.

Work:

- Add named material-accounting records for consumed, transferred, or preserved
  material.
- Track per-plate continental mass, oceanic mass, and tagged feature drift.
- Keep terrain elevation and visual beauty out of scope.

Exit gate:

- Global CAF and per-plate mass changes reconcile exactly with event records.
- No material class changes occur without a named event.
- Same-seed replay reproduces event logs and post-event state hashes.

## Slice 5: Resolution Scaling

Goal: run the accepted Phase II path at 60k, 100k, and 250k before considering
500k.

Work:

- Measure kernel timing separately from diagnostic/export timing.
- Export contact, label, filtered-overlap, unresolved-overlap, CAF, and area
  delta diagnostics.
- Compare against Phase I Stage 1.5 and Aurous failure memo rows where
  parameter-matched.

Exit gate:

- Qualitative outcome is stable across 60k/100k/250k.
- Runtime stays within the CarrierLab budget or the overage is documented as a
  Phase II finding.

## Stop Conditions

- Contact labels depend on previous global sample ownership.
- Centroid policy remains in the primary convergent-resolution path.
- Determinism breaks.
- Third-plate intrusion is folded into ordinary two-plate subduction.
- CAF or per-plate material changes without a named material event.
- Triangle filtering becomes a broad area-fill mask.
