# V2 Design Checkpoint

Date: 2026-06-08

Status: pre-code checkpoint for user review. No implementation approval is
implied.

## Scope

This checkpoint covers the pre-code V2 planning artifacts:

- `docs/rebaseline/v2-ptp-editor-tool-proposal.md`
- `docs/rebaseline/v2-source-anchor-table.md`
- `docs/rebaseline/v2-state-transition-model.md`
- `docs/rebaseline/v2-fixture-catalog.md`
- `docs/rebaseline/v2-metric-schema.md`

It does not modify production code. It does not run Unreal. It does not approve
Stage V2-0 implementation.

## Goal Restatement

CarrierLab's goal is to recreate the PTP/Cortial procedural tectonic planet
model in Unreal Engine 5 as an in-editor tool.

The immediate planning goal is narrower:

Design a small, source-grounded spine that can reveal whether the PTP-like
architecture survives integration before the codebase becomes large again.

## Artifact Coverage

| Requirement | Artifact | Current status |
| --- | --- | --- |
| Source-grounded architecture proposal | `v2-ptp-editor-tool-proposal.md` | present |
| Lifecycle/state model | `v2-state-transition-model.md` | present |
| Fixture catalog | `v2-fixture-catalog.md` | present |
| Metric schema | `v2-metric-schema.md` | present |
| Failure-trap analysis | proposal and checkpoint | present |
| Final go/no-go recommendation | this checkpoint | present |

## Blunt Findings

### Finding 1: V2 Is Worth Designing

Verdict: go for design.

Reason:

The source extractions contain enough detail to define the core carrier,
motion, projection, process-ordering, and remesh-filter contracts. The old
failures are no longer mysterious: the project repeatedly asked remesh and
resolver policies to do work that belongs to upstream process state.

### Finding 2: V2 Is Not Ready To Build Today

Verdict: no-go for immediate implementation.

Reason:

The artifacts are first-pass planning docs. They still need user review, and
the source/lab-policy table must be accepted before code starts. Building now
would recreate the old pattern: plausible code before the kill conditions are
fully agreed.

### Finding 3: The Current Implementation Path Should Not Be The Foundation

Verdict: do not keep patching current Phase III/IIIE as the V2 foundation.

Reason:

The current path has useful evidence and some successful gates, but it also has
an expanding policy stack around remesh, nearest-hit, mixed-material, distance
fallback, majority, component regularization, and crust-field stabilization.
That is exactly the shape of the old failure mode. V2 should reuse ideas only
after source/policy review, not inherit APIs or behavior.

### Finding 4: Standalone Remesh Is A Dead End

Verdict: no-go for standalone thesis remesh.

Reason:

The resampling extraction says the paper remesh consumes subduction/collision
state. Without that state, multi-hit resolution is source-silent and policy
sensitive. V2 remesh must be downstream of contact/process-state fixtures.

### Finding 5: The Editor Tool Must Come Late

Verdict: no-go for early in-editor polish.

Reason:

The user wants an in-editor tool, but the editor must wrap a gated spine. If
the actor/control-panel layer arrives before V2-0/V2-1/V2-2 metrics are stable,
it will add another place for visual self-agreement to hide core failure.

## Go/No-Go Matrix

| Question | Answer | Recommendation |
| --- | --- | --- |
| Can V2 Stage 0 be defined from sources? | yes | go for design review |
| Can V2 Stage 1 be defined from sources? | yes | go for design review |
| Can contact/process dry-run fixtures be defined? | yes | go for design review |
| Can full remesh be source-faithful without process state? | no | no-go |
| Can full remesh be designed after process state? | conditional yes | design only |
| Can editor UI be designed now as proof surface? | no | defer |
| Should current Phase III/IIIE code be patched forward as V2? | no | archive/evidence only |
| Should V2 implementation start now? | no | wait for review/go-no-go |

## Required User Review Questions

Before implementation, the user should explicitly accept or reject:

1. V2 starts as a new proof spine under `docs/rebaseline/`, not a patch of
   current Phase III/IIIE.
2. Stage 1.5 is retired as a standalone thesis-remesh gate.
3. The first code slice is numeric-only: no actor, no UI, no maps unless
   metrics pass.
4. Source-silent policies require explicit approval before they affect primary
   behavior.
5. Any fixture no-go stops the stage instead of triggering automatic patching.

## Implementation Boundary If Approved Later

First code slice after approval:

- add V2-only structs/classes in a new namespace or clearly named module surface
- implement only substrate, plate-local duplication, minimal material authority,
  and t=0 projection
- implement the first fixtures FX-000 through FX-004
- emit the V2 metric schema for V2-0
- write `docs/checkpoints/v2-stage-0-report.md`
- wait for explicit user go/no-go

Do not implement:

- rigid motion
- remesh
- gap fill
- subduction/collision
- rifting
- erosion
- editor actor/control-panel integration

## Stop Conditions For The Next Phase

Stop before or during V2-0 if:

- Stage 0 needs prior global owner data to pass
- ray projection cannot be separated from initialization labels
- boundary degeneracy cannot be separated from true overlap/miss
- local topology cannot be duplicated/re-indexed/recompacted deterministically
- metric rows are too weak to detect projection/output self-agreement

Stop before V2-4 if:

- process state cannot produce subducting/colliding marks before remesh
- q1/q2 cannot be implemented as continuous boundary provenance or explicitly
  demoted to lab-policy comparison
- mixed-vertex rebuild assignment policy is needed but unapproved
- multiple valid post-filter hits remain and the primary path tries to choose a
  winner anyway

## Current Recommendation

Recommendation: conditional go for the V2 design direction, no-go for coding
until the user reviews these artifacts.

This is the most conservative recoverable path:

- preserve current work as evidence
- use V2 docs as the new planning contract
- kill the architecture on tiny fixtures before it grows
- build the editor tool only after the spine survives numeric gates

If the user accepts this checkpoint, the next action is to revise these docs
from review feedback and then request explicit go/no-go for V2-0 implementation.
