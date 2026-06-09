# CarrierLab Milestone Roadmap

Date: 2026-06-09

Status: active go-forward structure.

This document replaces the mixed go-forward vocabulary of V2, Stage 1.5, Phase
IIIB, Phase IIIE, and similar labels. Those names remain in historical reports,
code filenames, and generated artifacts where changing them would create churn.
They are not the structure for new planning.

## Rule

Each future goal must name exactly one milestone.

Each milestone must define:

- the question it answers;
- the allowed scope;
- the forbidden scope;
- the evidence required to pass;
- the checkpoint report to write;
- the explicit user go/no-go required before advancing.

If a proposed task does not close the current checkpoint or prepare the next
milestone's entry packet, it waits.

## Current Position

The working bundle historically labeled V2 Stage 0 through V2 Stage 5 plus the
foundation stepper actor is treated as Milestone 0 closeout evidence. It is not
a precedent for future naming.

The Milestone 0 decision was not based on viewport appearance. The actor is
available as a diagnostic surface, but visual inspection is deferred unless it
reveals a usability problem or a mismatch with numeric evidence.

Old generated verdict names such as `GO_V2_6` are historical artifact labels.
The current active decision is:

```text
Milestone 1 entry packet -> user go/no-go -> Milestone 1 implementation
```

No new simulation behavior should be added before the Milestone 1 entry packet
is approved.

## Milestone 0: Foundation

Question: do we have the carrier authority model right?

Allowed scope:

- deterministic global substrate as query/resampling target;
- plate-local duplicated triangulations as moving authority;
- material carried by plate-local state;
- ray projection from current plate-local authority;
- numeric diagnostics for misses, overlaps, replay, forbidden fallbacks, and
  material provenance;
- optional editor inspection surface that reads existing evidence.

Forbidden scope:

- terrain beauty;
- elevation, erosion, slab pull, or rifting;
- persistent global sample ownership as authority;
- ownership repair, retention, hysteresis, anchoring, or prior-owner fallback;
- visual output as a pass gate.

Closeout evidence:

- build passes;
- automation passes;
- no high or medium claim-audit findings;
- checkpoint report states whether visual inspection is performed or deferred;
- explicit user go/no-go.

Current status: closed by user go/no-go on 2026-06-09.

## Milestone 1: Motion

Question: can plate-local authority move over time without losing coherence?

Allowed scope:

- rigid plate motion;
- repeated projection reads from moved plate-local triangulations;
- barycentric material transfer checks;
- drift, area, determinism, and performance metrics.

Forbidden scope:

- remeshing;
- subduction, collision, rifting, uplift, erosion, or terrain;
- gap fill;
- resolver policies that stand in for missing process state.

Entry requirement: a Milestone 1 entry packet that defines fixtures, scale
ladder, timing budget, drift gates, and stop conditions.

Current status: entry packet prepared in
`docs/checkpoints/milestone-1-entry-packet.md`; explicit user go/no-go is still
required before implementation.

## Milestone 2: Carrier Cycle

Question: can the carrier survive repeated motion plus scheduled resampling
without hidden repair?

Allowed scope:

- cadence;
- filtered sampling;
- q1/q2 gap fill;
- qGamma provenance;
- topology rebuild;
- process reset;
- conservation, replay, and performance gates.

Forbidden scope:

- terrain morphology;
- user-facing visual pass claims;
- fallback to prior global owner;
- centroid, random, or nearest resolver in the paper-faithful path.

Entry requirement: a Milestone 2 entry packet after Milestone 1 closeout.

## Milestone 3: Contact And Processes

Question: can the model classify contact into process state cleanly?

Allowed scope:

- convergence, divergence, and transform classification;
- subducting and colliding marks;
- contact/process ledgers;
- source-grounded process-state inputs for later remesh.

Forbidden scope:

- terrain response;
- material rescue hidden in process classification;
- UI-driven simulation correction.

Entry requirement: a Milestone 3 entry packet after Milestone 2 closeout.

## Milestone 4: Material Evolution

Question: can crust material evolve through tectonic events while preserving
provenance?

Allowed scope:

- oceanic generation;
- continental persistence and transfer;
- terrane and suture behavior;
- material conservation metrics.

Forbidden scope:

- terrain beauty as proof;
- global projected material as persistent authority;
- unreported lab policy in primary material behavior.

Entry requirement: a Milestone 4 entry packet after Milestone 3 closeout.

## Milestone 5: Terrain Response

Question: do tectonic processes produce plausible terrain fields from the
carrier state?

Allowed scope:

- uplift;
- erosion and dampening;
- elevation;
- age and thickness fields;
- map and viewport inspection tied to numeric evidence.

Forbidden scope:

- changing carrier authority to improve visuals;
- accepting morphology without carrier/process metrics;
- hiding paper/thesis gaps inside art direction.

Entry requirement: a Milestone 5 entry packet after Milestone 4 closeout.

## Milestone 6: Editor Tool

Question: can a user run, inspect, and diagnose the system inside Unreal
without the UI becoming simulation authority?

Allowed scope:

- actor and editor controls;
- step-through inspection;
- exports and reports;
- usability and observability.

Forbidden scope:

- UI state that repairs or mutates carrier authority outside the model;
- visual-only pass/fail decisions;
- editor affordances that obscure numeric gates.

Entry requirement: a Milestone 6 entry packet after Milestone 5 closeout, unless
a small diagnostic surface is explicitly needed earlier to inspect an already
defined numeric gate.

## Historical Mapping

Historical labels are preserved for traceability only:

| Historical label | Go-forward meaning |
|---|---|
| V2 | restart history and current worktree artifact names |
| Stage 0 | mostly Milestone 0 foundation evidence |
| Stage 1 | Milestone 1 motion evidence |
| Stage 1.5 | split across Milestone 2 carrier-cycle work and historical failure analysis |
| Phase II/III/IIIE | old integrated-path history, not go-forward structure |

Do not create new goals using the historical labels.
