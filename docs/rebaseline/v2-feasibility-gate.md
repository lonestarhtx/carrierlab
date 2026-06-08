# V2 Feasibility Gate

Date: 2026-06-08

Purpose: decide whether a clean V2 carrier restart is worth building before implementation begins.

This is a pre-code gate. It should be possible to fail this design on paper.

## What "Will Work" Means

This gate does not ask whether CarrierLab can quickly produce beautiful planet renders.

It asks whether we can recreate the PTP/Cortial carrier contract:

- moving plate-local material authority
- deterministic substrate and plate construction
- ray projection from current authority
- rigid within-window motion
- process-state-filtered remeshing
- divergent oceanic crust generation
- material continuity and replay metrics

If those can be made coherent, the project is worth rebuilding.

If those cannot be made coherent without hidden ownership repair, current-code carryover, or visual plausibility claims, stop.

## Current Pre-Code Verdict

Verdict: likely viable, but only with a narrow restart.

Why:

- The thesis gives enough detail for initial substrate, plate-local duplication, rigid motion, ray projection, resampling cadence, subduction/collision filtering, q1/q2/qGamma divergent fill, and plate rebuild.
- Prior failures are now explainable as ordering/architecture mistakes: remesh was explored before enough process state existed to resolve overlaps paper-faithfully.
- Rock 3 reinforces the same deeper principle: transported material authority must be separate from projection/output fields.
- The first proof slice is small enough to isolate: Stage 0 plus Stage 1, no remesh, no process mutation, no actor UI.

Main risk:

- Full remeshing cannot be honestly validated until convergence/subduction/collision state is designed first. If we try to make remesh standalone again, V2 will repeat the old failure.

## Fast Feasibility Questions

### Gate 1: Source Sufficiency

Question: Do PTP/thesis sources define enough behavior to build the next slice without invention?

Current answer:

- Stage 0: yes.
- Stage 1: yes.
- Pre-remesh convergence/filtering: partially, but enough to design a gate.
- Full remesh: yes only when upstream process state exists.
- Full downstream morphology/terrain: no, not for this restart's first proof.

Fail if:

- a stage requires guessing a primary physical rule
- a lab policy would be hidden inside primary behavior
- a report would need to say "probably paper-faithful" without source anchors

### Gate 2: Authority Separation

Question: Can every state variable be assigned to exactly one role?

Roles:

- substrate
- moving material authority
- plate carrier
- process state
- projection/output
- diagnostic metric

Current answer: yes, but only if code names enforce the separation.

Fail if:

- a global sample id is used as persistent material authority
- a projected owner is later consumed as truth
- a map/export is required to drive core behavior
- material provenance cannot be explained after a transition

### Gate 3: Dependency Order

Question: Can the design avoid asking a later stage to solve an earlier missing state?

Required order:

1. substrate
2. plate-local material authority
3. projection readout
4. rigid motion
5. convergence/contact state
6. subduction/collision filtering
7. remesh/gap fill
8. rifting/terrain/erosion

Current answer: yes, if Stage 1.5 is removed as a standalone proof stage.

Fail if:

- remesh appears before process-state filtering
- multi-hit tie-break policy appears in the paper-faithful path
- gap fill appears before divergent classification

### Gate 4: Minimal Proof Size

Question: Is there a small first implementation that could falsify the architecture quickly?

Current answer: yes.

The first proof only needs:

- a deterministic sphere substrate
- a tiny plate partition
- plate-local duplicate topology
- plate-local material records
- t=0 ray projection
- numeric hit/miss/overlap report

No UI, maps, erosion, climate, subduction, collision, or remesh.

Fail if:

- Stage 0 needs more than the substrate, plates, material, and projection
- visualization is needed to judge correctness
- a pass requires one-off special cases

### Gate 5: Known Failure Explanation

Question: Can the old failure modes be explained without assuming the paper is broken?

Current answer: yes.

Likely explanations:

- remesh tested without upstream convergence/subduction/collision filtering
- resolver policies substituted for process state
- gap/overlap cases drifted into geometry repair language
- observability grew faster than the core authority model

Fail if:

- the same failure mode survives in V2 despite correct dependency order
- Stage 0/1 fail before remesh/process complexity exists
- material continuity cannot be defined independently of projection output

## Fatal Unknowns

These can stop the restart before heavy implementation.

### Unknown 1: Boundary Degeneracy Policy

Risk:

- Exact edge/vertex ray hits can create candidate ambiguity even at t=0.

Design response:

- Treat as measured degeneracy, not failure.
- Define a deterministic half-open or classification-only policy before code.
- Non-degenerate misses/overlaps remain fatal.

Stop if:

- boundary degeneracy cannot be distinguished from real overlap/miss.

### Unknown 2: Continuous q1/q2 Boundary Provenance

Risk:

- The thesis implies nearest continuous boundary points, not nearest discrete samples.

Design response:

- Do not implement remesh until a continuous boundary-distance design exists.
- Allow a discrete approximation only as a named lab-policy comparison, not paper-faithful primary behavior.

Stop if:

- q1/q2 is reduced to nearest sampled owner without proof or explicit lab-policy status.

### Unknown 3: Process-State Filtering Before Remesh

Risk:

- Remesh correctness depends on subduction/collision marks created upstream.

Design response:

- Add `V2PreRemeshAudit` before any remesh commandlet.
- It must verify filtered-hit behavior on fixtures.

Stop if:

- remesh must pick among overlapping valid triangles without process marks.

### Unknown 4: Material Field Minimality

Risk:

- The first slice might under-store fields that later terrain/process stages need.

Design response:

- Store only required fields in Stage 0/1, but include stable provenance and extensible material records.
- Do not pretend omitted downstream fields are validated.

Stop if:

- adding required paper fields later would change authority semantics.

### Unknown 5: Performance Envelope

Risk:

- A faithful implementation may be mechanically correct but too slow.

Design response:

- Do not optimize first.
- Keep the query model swappable: brute-force oracle first, BVH acceleration later.
- Performance is a later reproduction outcome, not a Stage 0 blocker.

Stop if:

- correctness requires an optimization-specific data structure that changes semantics.

## Pre-Code Deliverables

Before coding V2, produce these four artifacts:

1. `v2-source-anchor-table.md`
   - Every V2 rule mapped to PTP, thesis, derived, or lab policy.

2. `v2-state-transition-model.md`
   - State lifecycle from substrate -> plate-local authority -> projection -> motion -> process -> remesh.

3. `v2-fixture-catalog.md`
   - Tiny synthetic cases for t=0 projection, boundary degeneracy, rigid motion, divergent gap, and filtered overlap.

4. `v2-metric-schema.md`
   - Required numbers for Stage 0, Stage 1, pre-remesh audit, and remesh.

If these cannot be written cleanly, do not code.

## Quick Go/No-Go Matrix

| Question | Current Answer | Gate |
| --- | --- | --- |
| Is Stage 0 source-complete? | Yes | Go |
| Is Stage 1 source-complete? | Yes | Go |
| Is standalone remesh source-complete? | No | No-go |
| Is remesh after process filtering source-complete? | Mostly yes | Conditional go |
| Can old failure modes be explained? | Yes | Go |
| Can material authority be separated from output? | Yes | Go |
| Can this be tested before maps/UI? | Yes | Go |
| Is full terrain/morphology recreation ready? | No | No-go for now |

## Recommended Decision

Proceed with pre-code V2 design, not implementation.

The project should pass or fail next on four documents:

- source anchors
- state transitions
- fixture catalog
- metric schema

Only after those are reviewed should code begin.

If those documents are coherent, the clean restart is worth building.

If they are not coherent, stop early and record the specific missing source rule or contradiction.
