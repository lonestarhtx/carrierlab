# V2 PTP Editor Tool Proposal

Date: 2026-06-08

Status: pre-code design proposal. This is not approval to implement V2.

## Goal

CarrierLab V2 should recreate the Cortial et al. Procedural Tectonic Planets
tectonic model in Unreal Engine 5 as an in-editor tool.

The immediate goal is not a beautiful planet. The immediate goal is a small,
source-grounded, end-to-end tectonic spine that can expose integration failure
before the codebase becomes large again.

The design target is:

1. Plate-local crust/material authority.
2. Rigid geodetic plate motion.
3. Projection from current authority to fixed global query samples.
4. Process state for convergence, subduction, collision, divergence, and
   remesh filtering.
5. Episodic remesh/resampling that consumes process state.
6. In-editor diagnostics that observe the model without becoming authority.

## Blunt Read

Current recommendation: proceed with V2 design, but do not build yet.

The project is recoverable if V2 is treated as a new proof spine. The current
Phase III/IIIE implementation path should be treated as evidence and a warning,
not as the foundation to keep patching. It contains useful measurements, but it
also contains the exact pattern the user called out: the system only becomes
obviously wrong after many subsystems have already been piled together.

The most dangerous mistake would be to restart at Stage 0 and then repeat the
same staged isolation trap. V2 must design the whole lifecycle first, then code
the smallest slice of that lifecycle.

## Non-Goals

V2 pre-code planning does not:

- continue patching the current Phase III/IIIE implementation path
- add Unreal editor UI, actors, control-panel behavior, or live visualization
- implement terrain beauty, erosion, climate, rendering, amplification, or
  Rock 3-style systems
- copy or port Rock 3 internals
- import Aurous sidecar, V6, V9, Prototype A/B/C/D/E, exporter, control-panel,
  or ownership-recovery logic
- advance a stage without a written checkpoint and explicit user go/no-go

## Source Authority

Normative sources for V2:

- `docs/paper-carrier-extraction.md`
- `docs/paper-resampling-extraction.md`
- `docs/phase-iii-paper-process-extraction.md`
- `docs/carrier-design.md`
- `docs/pre-mortem.md`
- Cortial et al., Procedural Tectonic Planets, local PDF under
  `docs/ProceduralTectonicPlanets/`
- Cortial thesis, local PDF under
  `docs/Synthese de terrain a lechelle planetaire/`

Historical evidence, not V2 authority:

- current Phase III/IIIE checkpoints
- current commandlets and actor code
- Rock 3 clean-room/metadata notes
- Aurous failure records

Rock 3 can inform vocabulary and diagnostics, but it does not define V2
behavior. Current CarrierLab can inform fixture design, but it does not define
V2 behavior.

## Architecture Spine

The minimal PTP-like system is:

```text
static global substrate
-> initial plate partition
-> plate-local duplicated triangulations
-> plate-local crust/material fields
-> rigid plate motion
-> projection/readout to fixed global samples
-> convergence/contact state
-> subduction/collision state
-> filtered remesh sampling
-> divergent q1/q2/qGamma oceanic generation
-> rebuilt plate-local triangulations
-> diagnostics and editor presentation
```

This is the full lifecycle. The first implementation will cover only the first
few entries, but the later entries must be designed now so that early choices do
not paint the project into a corner.

## Ownership Model

| Layer | Owns | Does not own |
| --- | --- | --- |
| Global substrate | deterministic samples, spherical TDS, area weights, query/remesh target topology | material transport, persistent plate ownership |
| Plate carrier | geodetic motion, duplicated local topology, moving vertex positions | visual output truth |
| Material authority | crust class, elevation/thickness fields, oceanic age, fold/ridge vectors, provenance | projected map colors |
| Process state | convergence fronts, subduction marks, collision candidates/events, divergence/remesh inputs | permanent sample ownership |
| Projection output | raw hits, selected hit, miss/overlap classes, diagnostic layers | upstream tectonic decisions |
| Editor tool | run controls, reports, maps, actor visualization | hidden correction of simulation state |

The phrase "global sample owner" is allowed only as a transient projection or
remesh assignment. It is forbidden as persistent tectonic authority.

## Stage Shape

V2 should not treat the old Stage 1.5 as a standalone thesis-remesh stage.

Recommended V2 stages:

| Stage | Purpose | Hard boundary |
| --- | --- | --- |
| V2-A | pre-code contract lock | no code |
| V2-0 | cold-start carrier | no motion, no mutation |
| V2-1 | rigid motion/readout | no remesh, no process mutation |
| V2-2 | contact/process-state dry run | no topology mutation |
| V2-3 | subduction/collision mutation fixtures | no remesh until filters pass fixture gates |
| V2-4 | paper remesh integration | consumes process state; no standalone tie-break rescue |
| V2-5 | editor tool integration | observes and controls the already-gated spine |

This ordering intentionally makes integration risk visible earlier than the old
path. Remesh is not allowed to appear before the process state it depends on.

## First Falsifiable Implementation Slice

The first code slice, when approved later, should be smaller than the existing
system:

- deterministic global substrate
- tiny initial plate partition
- duplicated/re-indexed plate-local topology
- minimal plate-local material records
- center-to-sample ray projection against plate-local triangles
- raw hit/miss/overlap/boundary metrics
- same-seed replay hashes

No maps unless numeric gates pass. No actor. No UI. No remesh. No process
mutation.

If this tiny slice cannot pass, the project should stop before rebuilding the
larger tool.

## Integration Failure Traps

These traps are the reason V2 exists.

### Trap 1: Stage Isolation Lies

Old pattern: build Stage 0, Stage 1, Stage 1.5, and Phase III as isolated
proofs, then discover that remesh requires process state that the isolated stage
did not have.

V2 rule: every stage must declare which later lifecycle state it will require
and which assumptions are temporary.

### Trap 2: Diagnostics Become Behavior

Old pattern: centroid, synthetic, nearest-hit, distance fallback, majority, and
similar policies start as diagnostics and slowly become the path that makes the
run complete.

V2 rule: every rule is classified as `source_explicit`, `source_implicit`,
`derived`, `source_silent_lab_policy`, or `forbidden`. Lab policy cannot become
paper-faithful by repetition.

### Trap 3: Projection Output Becomes Authority

Old pattern: projected sample ids or map state become tempting because they are
convenient and stable.

V2 rule: projection output can be consumed only by diagnostics, reports, and
editor visualization. State mutation must read plate-local authority and named
process state.

### Trap 4: Pretty Maps Hide Invalid State

Old pattern: visual artifacts are useful but arrive too early, making a broken
state feel salvageable.

V2 rule: maps are secondary evidence. A map is accepted only when tied to a
metric row and an authority source.

### Trap 5: Remesh Is Asked To Solve Missing Tectonics

Old pattern: remesh is asked to preserve continental material, resolve overlap,
fill gaps, and regularize plate ids before collision/subduction/rifting state is
fully present.

V2 rule: remesh consumes state; it does not invent state. If it needs to choose
between overlapping valid triangles without subduction/collision marks, that is
a no-go or a named comparison run, not the paper path.

### Trap 6: Code Mass Becomes The Argument

Old pattern: after enough commandlets and report rows exist, the project feels
too expensive to question.

V2 rule: each stage has a kill condition. If a condition fails, the recommended
action is checkpoint, redesign, or stop, not another patch by default.

## Required Design Artifacts

The proposal is only useful if paired with falsification artifacts:

- `docs/rebaseline/v2-source-anchor-table.md`
- `docs/rebaseline/v2-state-transition-model.md`
- `docs/rebaseline/v2-fixture-catalog.md`
- `docs/rebaseline/v2-metric-schema.md`
- `docs/rebaseline/v2-design-checkpoint.md`

Together they answer:

1. What is sourced?
2. What state exists?
3. What tiny cases break the design?
4. What numbers decide pass/fail?
5. Should we build?

## Preliminary Go/No-Go

Preliminary verdict: conditional go for design, no-go for implementation today.

Go only if the four falsification artifacts stay internally consistent.

No-go if:

- V2 requires persistent global ownership to pass Stage 0/1
- remesh must choose among multiple valid hits without process marks
- q1/q2 is reduced to sampled owner lookup without lab-policy status
- material accounting requires hidden repair/backfill
- editor visuals are needed to explain a pass
- the artifact set cannot state the full lifecycle without contradiction
