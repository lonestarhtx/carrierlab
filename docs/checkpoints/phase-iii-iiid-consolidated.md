# Phase III Sub-Phase IIID Consolidated Checkpoint

Status: CONDITIONAL CLOSE / HANDOFF. IIID.1-IIID.7 implement the continental-collision tracking, grouping, slab-break, suture, topology-mutation, and uplift pipeline for exercised fixtures. IIID.8 did not pass the planned Slice 5.5 uniform-oceanic-loss reduction gate. Phase IIID may be treated as implementation-complete for its local collision/suture surfaces, but it must not be reported as having solved the Slice 5.5 asymmetry or Stage 1.5 remesh/material-preservation problem.

Latest IIID evidence commit before this consolidation: `269833c92f721142cb85d7a008b7423f1b993682`

## Sub-Slice Summary

| Slice | Result | Evidence |
|---|---:|---|
| IIID.1 Terrane Component Detection | pass | Read-only connected-component expansion finds continental terrane components from collision seeds and keeps lab multi-hit policy dormant. |
| IIID.2 Candidate Grouping And 300 km Gate | pass | Collision candidates are grouped deterministically by plate pair and gated by interpenetration threshold. |
| IIID.3 Collision Event Selection | pass | One collision event is selected deterministically for a timestep; additional candidates remain deferred. |
| IIID.4 Slab Break Plan | pass | Source terrane removal plan is deterministic and topology-valid in the exercised fixture. |
| IIID.5 Suture Plan | pass | Destination attachment plan adds exactly the removed terrane triangles and preserves manifold topology in the exercised fixture. |
| IIID.6 Detach + Suture Mutation | pass | First IIID topology mutation is deterministic, mass-conserving, invalidates convergence tracking, and applies one collision per timestep. |
| IIID.7 Collision Uplift | pass | Thesis page-60 uplift formula is applied with zero oracle residual in the exercised fixture. |
| IIID.8 Slice 5.5 Asymmetry Recheck | fail / investigation | IIID-active 60k replay 0 transferred `6.340432106644` continental area across 10 collision events but uniform-oceanic single-hit net worsened from `-0.518782` to `-0.594389330059`. Active replay 1 was not run for this investigation checkpoint. |

## Consolidated Gates

| Gate | Result | Evidence |
|---|---:|---|
| IIIB independent signature inherited | pass | IIID slice commandlets use computed-vs-expected `bf8818a26ed7b1dc` checks where they touch convergence tracking. |
| Collision pipeline local determinism | pass | IIID.1-IIID.7 reports show replay-stable fixture hashes for detection, grouping, plans, mutation, and uplift. |
| Topology mutation conservation | pass | IIID.6 source/destination continental deltas are equal and opposite in the exercised fixture: `-0.003769911184` and `0.003769911184`. |
| Collision uplift oracle | pass | IIID.7 formula oracle residual is `0.000000000000000 km` in the exercised fixture. |
| Lab multi-hit policy dormant in IIID fixtures | pass | IIID reports gate policy-resolved multi-hit counts at zero. |
| Slice 5.5 uniform-oceanic loss reduced by IIID | fail | IIID.8 replay 0 shows `-14.57%` reduction, meaning the loss increased. |

## What IIID Establishes

IIID establishes a deterministic, plate-local continental-collision pipeline for the fixtures exercised so far:

- Collision evidence can be detected and grouped without changing carrier authority.
- Connected terrane components can be planned for detachment and suture.
- A single collision event can be applied as a plate-local topology mutation.
- The source plate loses exactly the terrane area that the destination plate gains in the exercised mutation fixture.
- Boundary and convergence tracking are invalidated after mutation for recomputation.
- Collision uplift applies the thesis formula as scalar elevation/fold state without moving unit-sphere vertices.

## What IIID Does Not Establish

IIID does not establish that the old Phase II filtered-resampling event is an adequate proof target for continental persistence. IIID.8 shows the opposite: after substantial collision/suture transfer, the old Slice 5.5 uniform-oceanic source-triangle bucket still worsens.

This does not invalidate IIID.1-IIID.7. It means the original IIID.8 hypothesis was measuring through the wrong downstream path. The thesis remesh algorithm filters subducting and colliding triangles before ray-hit selection, uses continuous q1/q2 provenance for divergent gaps, rebuilds plate-local topology, and resets process state. That path is IIIE, not IIID.

## Required Handoff To IIIE

IIIE must inherit the following concrete questions:

- Implement paper remesh candidate selection that ignores subducting and colliding triangles before choosing a source triangle.
- Replace Stage 1.5 endpoint/midpoint q1/q2 authority with continuous boundary-edge q1/q2 provenance.
- Treat unresolved post-filter multi-hit cases as fail-loud anomalies rather than centroid/random policy resolutions.
- Rebuild plate-local topology from the global TDS partition and reset convergence/subduction process state.
- Re-run a successor to IIID.8 after IIIE.3/IIIE.5, because the old Phase II filtered-resampling comparison is now known to be an insufficient closure metric.

## Consolidation Decision

Go for closing Phase IIID as a local collision/suture implementation sub-phase.

No-go for claiming Phase IIID solved the Slice 5.5 asymmetry, Stage 1.5 material preservation, or the thesis remesh contract.

Next safe move: Phase IIIE.1 remesh contract audit and Stage 1.5 reframe, with IIID.8 cited as the reason the remesh path must become the primary closure surface.
