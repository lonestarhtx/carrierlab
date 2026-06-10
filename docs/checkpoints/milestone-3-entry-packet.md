# CarrierLab Milestone 3 Entry Packet

Date: 2026-06-09

Status: prepared after Milestone 2 external review and hardening. Recommended
verdict: `READY_FOR_USER_GO_NO_GO_TO_PLAN_MILESTONE_3`.

## Question

Can process-state filters turn convergent overlap from a blocked/deferred M2 case
into a source-grounded write-back path without reintroducing ownership repair,
prior-owner fallback, or a deterministic overlap winner policy?

Milestone 3 exists because M2 deliberately stopped before convergent overlap
material selection. M2 exercised the carrier cycle for single-hit transfer and
divergent q1/q2 gap fill, but it also left blocked overlap samples as topology
holes. M3 must close that loop before any multi-window convergent run is treated
as valid.

## Entry Evidence

Milestone 2 is accepted as a carrier-cycle baseline with conditions:

- fixed global samples remain query/resample targets, not tectonic authority;
- plate-local triangulations remain the moving material authority;
- single-hit barycentric write-back is deterministic and replay-stable;
- divergent zero-hit q1/q2/qGamma generation is continuous and geometric;
- nondegenerate cross-plate overlaps are blocked until process filters exist;
- M2 now reports deferred overlap mass, unassigned-triangle budgets, split
  boundary-overlap counters, full resample-cycle timing, and M2 hash baselines.

## Allowed Scope

Milestone 3 may add:

- contact evidence needed to classify convergent overlap sources;
- local material/polarity rules needed to decide which side is subducting or
  otherwise invalid as a write-back source;
- triangle or hit filters consumed by resample write-back;
- literal M2 fixture reruns against pinned baselines, plus a default-off M3
  process-filter characterization path;
- reset and provenance counters for process marks at rebuild boundaries;
- multi-window convergent fixtures after the tripwire below exists.

## Forbidden Scope

Milestone 3 must not add:

- uplift, elevation, erosion, amplification, terrain beauty, or rendering goals;
- slab pull, rifting, plate birth/death, or long-term velocity feedback;
- persistent global sample ownership as authority;
- prior-owner, retention, repair, backfill, centroid, random, or nearest-hit
  overlap winners;
- q1/q2 gap fill for non-divergent holes; previously blocked sample ids may be
  measured in fixtures, but they must not be consulted as write-back authority.

## Mandatory Tripwire

M3 must include a fixture that is expected to fail under the M2 overlap policy
and pass only after process filters are active:

1. Create a convergent overlap sample in window 1.
2. Record that the sample was blocked because process state was unavailable.
3. Run a second resample window with motion.
4. Fail if the measured previously blocked sample is classified as a zero-hit
   divergent gap and written as fresh q1/q2 oceanic material.
5. Pass only when process filtering removes the subducting/invalid hit and
   produces a single source-grounded write, or when the current q1/q2 boundary
   pair fails a stateless divergence predicate and the sample remains deferred
   without oceanic gap conversion.

This tripwire protects against the M2 hole-to-oceanic conversion pump.

## Required Metrics

M3 must report:

- literal M2 pinned-baseline hash comparison;
- process-filter enabled/disabled state;
- contact/convergence candidate counts;
- local material evidence used for polarity;
- subducting or invalid hit count;
- filtered overlap-to-single-source write count;
- still-unresolved overlap count;
- q1/q2 divergence-rejected count;
- previously blocked samples that became q1/q2 oceanic, required to be zero;
- deferred overlap mass before and after filtering;
- unassigned triangle count and budget;
- material delta and total-variation delta, with explicit gated versus
  characterization status;
- per-step process timing and per-window resample-cycle timing as separate
  numbers.

## Performance Anchor

M3 must keep two timing lanes separate:

- per-step motion/contact/process timing is compared to the paper total-row
  style budget;
- per-window resampling/rebuild timing is compared to the paper resampling row
  where a matching row exists.

Do not treat M2's narrow 1.24 s step-kernel pass as M3 headroom. M2's useful
performance evidence is that the 250k full resample cycle is below the paper's
3.58 s resampling-row anchor on this machine.

## Exit Gate

Milestone 3 passes only if:

- literal M2 fixture reruns match the pinned M2 baseline hashes;
- the hole-to-oceanic tripwire passes;
- process-enabled overlap write-back has source-grounded filtered evidence;
- forbidden resolver/fallback counters remain zero and are source-audited;
- a multi-window resolvable-polarity convergent fixture with motion does not
  grow hole count across windows;
- scale rows label conservation and topology honestly as gated or
  characterization;
- a checkpoint report is written and the user gives explicit go/no-go before the
  next milestone.
