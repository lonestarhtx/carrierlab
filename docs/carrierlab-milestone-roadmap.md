# CarrierLab Milestone Roadmap

Date: 2026-06-10

Status: active go-forward structure after the Milestone 3 remediation external
review. Milestone 3 remediation (`88421d3`) reports `MILESTONE_3_PASS` and was
accepted by external review. The Milestone 4 entry packet is prepared, but M4
implementation still requires explicit user go/no-go.

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

M0 through M3R are the current foundation chain. M3R is implemented, pushed, and
externally accepted. The next local decision is whether to approve the
Milestone 4 entry packet, not whether to skip directly into M4 implementation.

The milestone structure is:

1. M0: Foundation.
2. M1: Motion.
3. M2: Carrier Cycle.
4. M3: Contact Evidence And Process Filters.
5. M4: Paper Crust Fields And Per-Step Tracking.
6. M5: Process Mutation And Feedback.
7. M6: Editor Validation Tool.
8. M7: Amplification.

The Milestone 0 decision was not based on viewport appearance. The foundation
stepper actor is available as a diagnostic surface, but visual inspection is
not a pass gate.

Old generated verdict names such as `GO_V2_6` are historical artifact labels.
Do not create new goals using historical labels.

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

Current status: closed by user go/no-go on 2026-06-09.

## Milestone 1: Motion

Question: can plate-local authority move over time without losing coherence?

Allowed scope:

- rigid plate motion;
- repeated projection reads from moved plate-local triangulations;
- barycentric material transfer checks;
- static per-plate AABB trees queried through inverse-transformed rays;
- drift, area, determinism, and performance metrics.

Forbidden scope:

- remeshing;
- subduction, collision, rifting, uplift, erosion, or terrain;
- gap fill;
- resolver policies that stand in for missing process state.

Current status: implemented and remediated after external review. The closeout
report lives in `docs/checkpoints/milestone-1-closeout-report.md`. The 50k and
250k gates pass, the 100k characterization probe passes, and the 500k stretch
probe remains a performance watch rather than a closed requirement.

## Milestone 2: Carrier Cycle

Question: can the carrier survive scheduled resampling and topology rebuilds
without hidden repair?

Allowed scope:

- cadence;
- global substrate sampling as a query pass;
- q1/q2 gap fill and qGamma provenance;
- topology rebuild;
- process reset at rebuild boundaries;
- conservation, replay, and performance gates.

Forbidden scope:

- terrain morphology;
- user-facing visual pass claims;
- fallback to prior global owner;
- centroid, random, or nearest resolver in the primary path;
- material consequences that require subduction/collision rules.

Current status: implemented and externally reviewed with conditions folded into
M3/M3R. The closeout report lives in
`docs/checkpoints/milestone-2-closeout-report.md`. M2 introduced the hole/debt
accounting that M3R now gates more precisely.

## Milestone 3: Contact Evidence And Process Filters

Question: can the model classify contact into process state cleanly, then use
that process state as a conservative resampling filter?

Allowed scope:

- source-grounded contact evidence from plate-local shared-source boundaries;
- plate-interior probes for signed opening and contact-local material;
- convergence, divergence, and transform/low-margin classification;
- subducting and colliding triangle labels;
- q1/q2 rejection split by opening-rate, process-filter, and same-plate cause;
- process-filtered resampling decisions;
- replay, sign, polarity, pump-safety, and performance gates.

Forbidden scope:

- terrain response;
- material rescue hidden in process classification;
- UI-driven simulation correction;
- persistent contact/ridge identity across rebuilds;
- prior-owner, centroid, random, or nearest winner resolution for unresolved
  multi-hit samples.

Current status: M3R implemented and pushed at `88421d3`. The regenerated
closeout report lives in `docs/checkpoints/milestone-3-closeout-report.md` and
reports `MILESTONE_3_PASS`. External review accepted M3R and authorized M4
planning.

## Milestone 4: Paper Crust Fields And Per-Step Tracking

Question: can the carrier carry the paper's crust fields and track convergence
through time without inventing persistent ownership?

Entry evidence before implementation:

- M3R accepted after external review;
- a paper-regime characterization run using existing features only;
- plate speeds up to the paper regime (`v0 = 100 mm/yr`);
- speed-driven-equivalent cadence characterization, about 16-64 steps per
  window;
- at least 8 windows at 50k;
- report deferral debt, hole dynamics, miss scaling, runtime, and class
  distributions.

The characterization run sets M4 gate values. It is not itself a proof of M4.

Entry packet: `docs/checkpoints/milestone-4-entry-packet.md`.

Allowed scope:

- inert crust fields first: elevation `z`, oceanic age `a_o`, ridge direction
  `r`, and fold direction `f`;
- barycentric field transfer through resampling;
- vector-field rotation with plate motion, checked by an independent rotation
  oracle;
- q1/q2 oceanic generation setting age to zero and ridge direction from the
  paper form `r = (p - qGamma) x p`;
- per-step convergence tracking within a resampling window;
- active boundary lists derived from current boundary triangles;
- `d(p)` accumulation with an independent oracle;
- per-window subduction matrix;
- live O/C polarity from M3R's interior-side material;
- O/O polarity from real oceanic age, with equal age deferred;
- speed-driven cadence: `DeltaT = (1-alpha)M + alpha m`,
  `alpha = min(1, vmax/v0)`, `m = 32 Ma`, `M = 128 Ma`;
- cross-window front continuity measured from re-derived geometry.

Forbidden scope:

- any contact, ridge, or front ID that survives rebuild as authority;
- any registry that functions like hidden ownership;
- material mutation beyond the existing M2/M3 resampling paths;
- uplift, terrain response, rifting, slab pull, or plate-count mutation;
- treating measured cross-window continuity as proof that identity persisted.

Evidence required to pass:

- field-inert baselines remain byte-identical when fields are unused;
- vector fields rotate with plate motion within the declared tolerance;
- `d(p)` accumulation matches an independent raw-input oracle;
- O/O age-polarity fixture swaps correctly and equal-age defers;
- front-continuity ratio is above a pre-registered threshold across multiple
  windows;
- multi-window scale row inherits M3R sign, polarity, q1/q2, and pump-safety
  gates;
- M3R content-stream hashes are pinned and rerun.

Checkpoint report: `docs/checkpoints/milestone-4-closeout-report.md`.

## Milestone 5: Process Mutation And Feedback

Question: can tectonic processes mutate crust state faithfully, then survive
feedback, without rescue logic?

Milestone 5 is one milestone with sub-phases. Each sub-phase requires its own
entry packet, closeout report, and explicit user go/no-go.

### M5A: Subduction Consequences

Allowed scope:

- visible/historical elevation split at marked triangles;
- trench elevation;
- overriding-plate uplift from raw geometry and process state;
- per-step elevation evolution;
- continental erosion, oceanic dampening, sediment accretion;
- oceanic age advancing with time;
- material event ledger with every mass delta tied to an event class.

Evidence required to pass:

- independent analytic oracles recompute uplift and evolution from raw inputs;
- conservation reconciles per event class, not only in aggregate;
- bit-exact-zero residuals are treated as suspicious unless the equality is
  algebraically expected.

### M5B: Collision And Terrane Transfer

Allowed scope:

- terrane detection;
- per-pair deduplication;
- interpenetration gating driven by M4 `d(p)`;
- opposing-mass threshold;
- slab break and suture as topology/material transfer;
- fold-direction increments.

Evidence required to pass:

- one collision per step unless explicitly broadened by a later packet;
- three-way area/mass conservation for transfer;
- no fallback owner or nearest/centroid winner.

### M5C: Thesis-Rule Restoration

Allowed scope:

- retire the temporary `source_conflict` deviation once terrane transfer and
  fields are live;
- resample with the thesis-style rule: zero valid hits after filtering goes to
  q1/q2;
- drain filter-exhausted, C/C, and O/O-ambiguous deferral classes to zero;
- preserve M5B behavior as an off-baseline.

Evidence required to pass:

- deferred-debt ledger reads zero at scale;
- unassigned triangles are zero at scale;
- the guard predicate remains present but should not fire on filtered samples.

### M5D: Population Feedback

Allowed scope, each default-off with an off-baseline:

- slab pull;
- rifting;
- fully subducted plate destruction;
- plate-count and motion feedback.

Evidence required to pass:

- disabled mirrors preserve the previous baseline;
- enabled feedback has independent oracles and hard clamps;
- plate-count equilibrium is measured, not assumed.

M5 exit evidence:

- long-horizon tectonic-only run under paper cadence and speed regime;
- 100 or more windows, or a pre-registered shorter run justified by runtime;
- plate-count equilibrium under rifting and destruction;
- age-field structure: young near ridges, old near trenches;
- clean conservation ledger;
- runtime compared against the paper's relevant lanes;
- numeric macro-morphology checks, not visual acceptance.

Checkpoint report: `docs/checkpoints/milestone-5-closeout-report.md`, plus
sub-phase reports.

## Milestone 6: Editor Validation Tool

Question: can a user run, inspect, and diagnose the validated foundation inside
Unreal without the UI becoming simulation authority?

Allowed scope:

- step/window-granular stepping;
- read-only map layers for elevation, age, ridge/fold direction, subduction
  mask, `d(p)`, contact classes, and deferred/event ledgers;
- per-lane performance telemetry;
- export of each view's backing numbers;
- usability and observability.

Forbidden scope:

- UI state that repairs or mutates carrier authority outside the model;
- visual-only pass/fail decisions;
- editor affordances that obscure numeric gates.

Evidence required to pass:

- an editor-stepped run produces byte-identical content hashes to the commandlet
  run of the same config;
- exported views trace back to commandlet metrics or explicit read-only runtime
  metrics.

Checkpoint report: `docs/checkpoints/milestone-6-closeout-report.md`.

## Milestone 7: Amplification

Question: can visual amplification make validated tectonic fields more legible
without becoming simulation authority?

Allowed scope:

- detail amplification;
- oceanic amplification oriented by ridge direction;
- age-driven amplitude;
- transform-fault structure perpendicular to ridges;
- high-frequency underwater detail;
- exemplar-based continental amplification only after the data dependency is
  explicitly accepted.

Forbidden scope:

- feeding amplified fields back into carrier authority;
- using prettiness to replace Milestone 0-6 numeric gates;
- introducing new tectonic process rules outside a written entry packet;
- changing carrier behavior to improve visuals.

Evidence required to pass:

- carrier hashes are unchanged by rendering or amplification;
- resolution/performance ladder is compared against the paper's amplification
  claims;
- Figure-style comparisons are treated as diagnostic review artifacts, not as
  authority.

Checkpoint report: `docs/checkpoints/milestone-7-closeout-report.md`.

## Standing Rules

Physics gates are mandatory wherever physics enters. Every milestone that adds a
physical semantic must ship sign, direction, or class-distribution hard gates
and at least one conservation, symmetry, or independent-oracle cross-check.

Pinned predecessor baselines are mandatory. Each milestone reruns literal
predecessor configs and pins predecessor content-stream hashes as constants.
Metrics hashes can be reported but should not be the only pinned artifact.

Feedback defaults off. Slab pull, rifting, cadence feedback, plate destruction,
or any other motion/topology/authority feedback must ship disabled mirrors and
off-baselines before it can be enabled.

Independent oracles are required for thesis formulas. Oracles recompute from raw
inputs, not from mutated outputs. Bit-exact-zero residuals are a warning unless
the equality is an explicit algebraic identity.

Deviation ledgers must name restoration preconditions. Any accepted lab policy
or paper/thesis deviation must state when and how it will be retired.

Budgets and lane definitions are pre-registered in entry packets. Changing a
gated performance or correctness definition after results is itself a finding.

Every entry packet must include a carry-over tracker. Open items must be closed,
accepted as explicit risk, or made blocking. Current carry-overs for the M4
entry packet are:

- M3R external acceptance review result;
- M2-FX-006 prior-owner negative-control disposition remains a disclosure, not
  an active detector;
- sqrt(N) miss-scaling characterization, expected to be revisited by the M4
  paper-regime characterization run;
- process-lane definition nit from M3 review.

## Historical Mapping

Historical labels are preserved for traceability only:

| Historical label | Go-forward meaning |
|---|---|
| V2 | restart history and current worktree artifact names |
| Stage 0 | mostly Milestone 0 foundation evidence |
| Stage 1 | Milestone 1 motion evidence |
| Stage 1.5 | split across Milestone 2 carrier-cycle work and historical failure analysis |
| Phase II/III/IIIE | old integrated-path history, not go-forward structure |
| Amplification | Milestone 7 visual legibility work after editor validation |

Do not create new goals using the historical labels.
