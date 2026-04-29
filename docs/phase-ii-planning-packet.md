# CarrierLab Phase II Planning Packet

Status: draft for user review. Phase II implementation may not start until the
user records an explicit go/no-go against this packet.

## Packet Files

- `docs/phase-ii-subduction-design.md`: ADR-shaped design contract.
- `docs/phase-ii-pre-mortem.md`: ranked failure modes and required controls.
- `docs/phase-ii-slice-plan.md`: implementation slices and gates.

## Grounding

Phase I answered the carrier-foundation question far enough to move on:

- Stage 0 established cold-start spherical Delaunay coverage.
- Stage 1 established rigid plate-local motion and analytic drift coherence.
- Stage 1.5 characterized resampling under centroid, synthetic, and random
  multi-hit policies.

The accepted Stage 1.5 interpretation is not "resampling is done." It is:
the carrier foundation is characterized, while convergent multi-hit behavior is
process-coupled. Longer Stage 2 runs would mostly amplify that same equilibrium.
The next useful experiment is therefore a subduction prototype that supplies the
triangle-filter state the thesis resampling algorithm expects.

## Phase II Goal

Determine whether explicit, deterministic subduction process state can replace
Stage 1.5's lab-only multi-hit policies and stabilize convergent-zone resampling
without contaminating the Phase I carrier authority model.

The success condition is narrow:

- contact detection is deterministic and geometry-derived
- triangle process labels are traceable to contacts
- resampling filters only explicitly labeled subducting triangles
- unresolved or ambiguous contacts remain visible
- material and plate-area changes reconcile with named event records

## Non-Negotiables

- Plate-local crust geometry remains carrier authority.
- Global samples remain projection/resampling targets, never persistent tectonic
  authority.
- Centroid/random tie-breaks remain comparison controls only.
- No terrain beauty, uplift, slab pull, rifting, erosion, or game integration.
- No Aurous V6/V9/Prototype A/B/C/D/E port.
- Every slice ends with a written checkpoint and explicit go/no-go.

## Review Questions

1. Is the Phase II goal correctly scoped to subduction as the missing
   convergent-resolution process state?
2. Do you approve Slice 0 as a no-mutation harness/spec-audit slice before
   contact detection lands?
3. Do you approve the conservative polarity policy: mixed contacts may label
   oceanic-under-continental, same-material contacts remain ambiguous unless a
   fixture supplies polarity?
4. Do you approve the initial gates: post-filter non-boundary multi-hit below
   2% at 60k, zero contacts in zero-motion/divergence, deterministic event logs,
   and no material change without a named event?
5. Do you want Phase II to stop after filter integration if that resolves the
   carrier question, or continue into named material consumption immediately?

## Proposed Approval Statement

If accepted, record:

`Phase II planning packet approved. Begin Slice 0 only. No subduction mutation or
terrain process is approved beyond the Slice 0 harness.`
