# V2 State Transition Model

Date: 2026-06-08

Status: pre-code lifecycle model.

Purpose: define what state exists, where authority lives, and how state may
change across the PTP-like lifecycle before V2 code begins.

## State Classes

| State class | Lifetime | Authority | Examples |
| --- | --- | --- | --- |
| Substrate state | whole run | geometric/query substrate only | Fibonacci sample positions, spherical TDS, area weights, neighbor graph |
| Plate carrier state | run, rebuilt at remesh | moving carrier authority | plate id, rotation axis, angular speed, local vertices, local triangles |
| Material state | run, transferred/mutated by named processes | crust/material authority | material class, elevation/thickness, oceanic age, ridge/fold vectors, provenance |
| Process state | bounded event/window lifetime | process authority | convergence fronts, subducting triangles, collision candidates, terrane events |
| Projection state | transient per readout | diagnostic/render output | raw hits, selected hit, miss class, overlap class, projected material |
| Report/editor state | transient/persistent artifact | observation only | metrics rows, hashes, maps, UI state |

## Lifecycle Overview

```text
S0: create static substrate
S1: partition substrate into initial plates
S2: duplicate/re-index/recompact plate-local meshes
S3: initialize plate-local material fields
S4: project global readout from plate-local authority
S5: rotate plate-local carrier and material vectors
S6: classify projection gaps/overlaps during motion
S7: detect convergence/divergence/contact process state
S8: mutate subduction/collision/obduction state where paper process says so
S9: remesh by sampling current authority through process filters
S10: generate divergent oceanic crust for zero-hit samples
S11: rebuild plate-local meshes and reset process state
S12: expose diagnostics to editor
```

The first implementation slice covers S0-S4 only. The design must still define
S5-S12 now because later stages constrain what S0-S4 are allowed to store.

## Transition Table

| Transition | Inputs | Output | Allowed mutation | Forbidden mutation | First V2 gate |
| --- | --- | --- | --- | --- | --- |
| T0 substrate build | seed, sample count | global samples, TDS, areas | create deterministic topology | material/plate authority on substrate | V2-0 |
| T1 initial partition | substrate, plate count, land coverage | initial plate domains | assign initial diagnostic domain and plate seed | persistent global owner as future authority | V2-0 |
| T2 local duplication | substrate triangles, plate domains | per-plate local vertices/triangles | duplicate, re-index, recompact, mark boundaries | reference global sample rows as live mesh | V2-0 |
| T3 material init | local vertices, plate/domain settings | plate-local material records | initialize material class and minimal fields | output-map material authority | V2-0 |
| T4 projection readout | substrate samples, plate-local meshes/material | projection records | compute raw hits and selected diagnostic readout | write back to carrier/material authority | V2-0 |
| T5 rigid motion | plate motion, dt | moved local vertices and rotated vectors | rotate plate-local geometry/material vectors | remesh, repair, fill, change material class | V2-1 |
| T6 motion readout | moved local meshes | projected miss/overlap classes | classify divergent/convergent/numeric/topology categories | mutate state to close coverage | V2-1 |
| T7 convergence tracking | boundaries, moved meshes, material fields | active fronts, polarity candidates | create auditable process state | consume material or topology before approved process | V2-2 |
| T8 subduction/obduction | process state, material fields | subducting marks, uplift/slab records | mark triangles, update allowed fields, mutate motion only through slab pull gate | hidden candidate tie-break | V2-3 |
| T9 collision | collision candidates, terrane sets | topology transfer/suture event | detach/suture continental terranes, named uplift | remesh-time continental preservation hack | V2-3 |
| T10 remesh pre-treatment | plate meshes, process marks | filtered plate boundaries/BVHs | exclude subducting/colliding triangles | choose among unfiltered candidates | V2-4 |
| T11 remesh sampling | substrate samples, filtered meshes | per-sample remesh records | barycentric copy from valid hit | prior-owner fallback | V2-4 |
| T12 divergent generation | zero-hit remesh records, q1/q2/qGamma | generated oceanic records | create oceanic crust with provenance | preserve old material without named collision/rift process | V2-4 |
| T13 rebuild | remesh assignments, global TDS | new plate-local meshes/material | duplicate/re-index/recompact; preserve plate motion | carry old process marks | V2-4 |
| T14 process reset | rebuilt plates | fresh process baseline | invalidate subduction/convergence state | persist old marks as ownership | V2-4 |
| T15 editor observation | reports, state snapshots | UI/maps/control readouts | display, export, run commandlets | alter simulation state to make maps pretty | V2-5 |

## Authority Flow

```text
Substrate -> Plate carrier construction
Plate carrier + Material state -> Projection readout
Plate carrier + Material state -> Process detection
Process state + Plate carrier + Material state -> Process mutation
Process marks + Plate carrier + Material state -> Remesh sampling
Remesh records -> Rebuilt plate carrier + rebuilt material state
Any state -> Diagnostics
Diagnostics -> no simulation authority
```

There is deliberately no arrow from projection output back into material
authority. Any future exception would be a design violation unless it is a
named paper process with source anchors.

## Minimal Material Record

V2-0/V2-1 material records should be minimal but future-proof:

| Field | Required by | Stage stored | Notes |
| --- | --- | --- | --- |
| material class | carrier, subduction/collision, gap fill | V2-0 | continental, oceanic, empty/unknown only if sourced |
| plate id | carrier association | V2-0 | association inside plate-local authority, not global sample owner |
| elevation or thickness proxy | later process/elevation gates | V2-0 as inert field or V2-2 if deferred | must not be used for terrain claim early |
| oceanic age | ocean-ocean polarity, divergent generation | V2-2 before process filtering | zero for generated ridge crust |
| ridge direction | divergent generation | V2-4 | can be absent until gap fill exists |
| fold direction | subduction/collision/uplift | V2-3 | rotate with plate once present |
| provenance | all gates | V2-0 | initial, projected, advected, generated, subducted, collided, sutured |
| validity flags | all gates | V2-0 | must separate numeric invalid from physical class |

## Process State Lifetime

| State | Created | Consumed | Reset |
| --- | --- | --- | --- |
| convergence frontier | T7 | T8, T10 | T14/remesh |
| subduction polarity matrix | T7/T8 | T8, T10 | T14/remesh |
| subducting triangle marks | T8 | T10 filter | T13/T14; subducted triangles do not survive |
| collision candidates | T7/T8 | T9 | after event or remesh |
| terrane transfer events | T9 | material/topology ledger | persist as audit history |
| divergent gap records | T11/T12 | rebuild/material ledger | persist as provenance on generated material |
| rifting candidates | later stage | rifting event | later stage only |

## Stage Contracts

### V2-0 Cold Start

Required transitions: T0-T4.

Must hold:

- local topology exists before projection
- projection records raw candidates
- non-degenerate miss count is zero
- non-degenerate overlap count is zero
- boundary-degenerate count is bounded and reported
- material authority and projected output are separate

Stop if:

- projection reads initial global owner as truth
- t=0 pass requires fallback/repair
- local topology cannot be rebuilt from global TDS without ambiguity

### V2-1 Rigid Motion

Required transitions: T5-T6.

Must hold:

- plate-local positions match independent analytic motion within tolerance
- material remains attached to moving plate-local vertices
- gaps/overlaps are classified, not repaired
- replay hashes are stable

Stop if:

- material continuity depends on projection output
- drift oracle reads mutated state as its expected value
- any remesh/gap fill appears

### V2-2 Contact/Process Dry Run

Required transition: T7.

Must hold:

- contact evidence is auditable per triangle/plate pair
- polarity candidates are recorded without mutation
- third-plate evidence is visible
- process state can explain later remesh filters

Stop if:

- convergence evidence collapses to sample owner labels
- unresolved overlap is hidden by centroid/random/nearest policy

### V2-3 Process Mutation Fixtures

Required transitions: T8-T9.

Must hold:

- subducting/colliding marks exist before remesh
- collision/terrane transfer has named event records
- slab-pull, if enabled, has on/off differential metrics

Stop if:

- continental preservation is implemented as remesh repair
- material is destroyed/created without named process

### V2-4 Paper Remesh Integration

Required transitions: T10-T14.

Must hold:

- subducting/colliding triangles are filtered before hit selection
- zero-hit samples become divergent generation records
- q1/q2/qGamma provenance is auditable
- new local topology is rebuilt deterministically
- process state is reset/inactivated after remesh

Stop if:

- multiple valid post-filter intersections require an uncited winner
- q1/q2 uses prior owner or projected owner as authority
- mixed-triangle assignment policy is unapproved

### V2-5 Editor Tool

Required transition: T15.

Must hold:

- editor controls cannot mutate state outside approved command paths
- maps cite the metric row they visualize
- no live actor behavior is a proof substitute

Stop if:

- UI state drives simulation correction
- user-facing visuals overclaim stage status

## Old Failure Mapping

| Historical failure | State-model diagnosis | V2 prevention |
| --- | --- | --- |
| Stage 1.5 standalone remesh fails conservation | T10-T12 ran without T7-T9 process state | remesh locked behind process-state gates |
| Policy stack grows to resolve multi-hit holds | POL-002/POL-003 were not isolated from primary behavior | source/lab-policy table required before code |
| Visual remesh applies but coherence remains suspect | T15 was too close to proof status | editor is observation only |
| Crust fields become unstable after remesh | material field authority lifecycle not explicit enough | material record and field sync gates exist before remesh claim |
| Global samples tempt authority retention | T4/T11 output assignments blur into state | no projection-to-authority arrow |
