# CarrierLab Milestone 2 Entry Packet

Date: 2026-06-09

Status: prepared after Milestone 1 review remediation. Recommended verdict:
`READY_FOR_USER_GO_NO_GO_TO_IMPLEMENT_MILESTONE_2`.

## Question

Can the carrier survive a scheduled motion/resampling cycle without smuggling in
ownership repair?

Milestone 2 is the first milestone where projection output may write back into
plate-local authority. That makes it higher risk than Milestone 1. The work is
allowed only if every write has a source-grounded reason: a single valid plate
intersection, or an explicit divergent gap fill using continuous q1/q2/qGamma
construction.

## Entry Evidence

Milestone 1 has now been remediated against the external review findings:

- the brute-force micro oracle scans canonical plate-local triangles in the
  same inverse-ray frame as the static AABB trees;
- `FX-012` is an asymmetric oracle-frame sentinel: corrected AABB/canonical
  brute-force classification matches, while the deliberately legacy moved-frame
  diagnostic disagrees;
- the commandlet can run a 100k scale probe between the 50k gate and 250k gate;
- miss and overlap populations now include top plate-pair attribution;
- the closeout report distinguishes configured fixture expectations from
  scale rows where the answer is `n/a`;
- forbidden fallback counters are documented as contract tripwires plus source
  inspection evidence, not as magical future-code detectors.

Current scale readout from the remediation run:

| scale | raw misses | miss % | raw overlaps | overlap % | step kernel ms | budget |
|---|---:|---:|---:|---:|---:|---|
| 50k | 577 | 1.1540 | 579 | 1.1580 | 379.877 | pass |
| 100k | 870 | 0.8700 | 907 | 0.9070 | 308.384 | pass |
| 250k | 1399 | 0.5596 | 1380 | 0.5520 | 818.381 | pass |
| 500k | 1872 | 0.3744 | 1874 | 0.3748 | 1725.403 | stretch fail |

The sublinear miss fraction is real across 50k/100k/250k/500k. It is no longer
a reason to distrust the broadphase, but it is a reason not to calibrate
Milestone 2 q1/q2 gates from a naive "misses scale linearly with N" assumption.

The 500k run is not a Milestone 1 blocker, but it is a Milestone 2 performance
watch. It exceeds the 1240 ms 250k-derived M1 step-kernel cap.

## Source Anchors

Milestone 2 is anchored to the paper/thesis carrier cycle:

- fixed global sampling is a resampling target, not material authority;
- moving plate-local triangulations store crust/material state;
- resampling casts from the planet center through global samples and reads
  plate-local barycentric data;
- zero-hit divergent gaps use q1/q2 boundary evidence and qGamma provenance;
- remesh rebuilds plate-local topology from resampled authority;
- process-state filtering is required before accepting subducting/colliding
  overlaps as material sources.

If the paper/thesis is silent, the implementation must label the decision as
lab policy in the report.

## Allowed Scope

Milestone 2 may add:

- a scheduled resample cadence, expressed in steps and total motion window;
- resample-boundary projection using the Milestone 1 inverse-ray static AABB
  query architecture;
- post-motion broadphase cap recomputation with explicit positive-margin gates;
- single-hit barycentric transfer from plate-local authority into the rebuilt
  plate-local carrier;
- divergent zero-hit q1/q2/qGamma gap fill with continuous geometric boundary
  evidence;
- plate-local topology rebuild/reindex/compact after accepted samples;
- process-ledger reset counters only for state that already exists in the lab
  harness;
- repeated lifecycle conservation metrics: area/mass accounting, material
  provenance, replay hashes, and field sharpness/variation across remesh;
- scale probes at 50k, 100k, 250k, and optional 500k.

## Forbidden Scope

Milestone 2 must not add:

- terrain, elevation, erosion, uplift, slab pull, or visual beauty;
- subduction, collision, rifting, or polarity decisions beyond counters needed
  to detect whether they stayed absent;
- persistent global sample ownership as authority;
- prior-owner fallback, retention, hysteresis, anchoring, recovery, repair, or
  backfill;
- centroid, nearest, random, or previous-owner selection as a primary overlap
  winner;
- discrete q1/q2 endpoint or midpoint authority when continuous boundary
  evidence is required;
- UI/editor state that mutates simulation authority;
- any "paper-faithful" claim for overlap handling before process-state filters
  exist.

## Resample Semantics

Milestone 1 projected every step as a falsification diagnostic. Milestone 2 must
switch to paper-style resample boundaries:

- motion may run for a configured window;
- projection/write-back happens only at the scheduled resample boundary;
- per-step diagnostic projection can remain as an optional report-only mode, but
  it must not write authority;
- the resample boundary must record total motion angle per plate and the exact
  query architecture used.

## Broadphase Invariants

The Milestone 1 static per-plate tree remains the default query architecture:

- build one canonical plate-local tree per plate at rebuild time;
- inverse-rotate the sample ray by the plate's total motion angle at the
  resample boundary;
- recompute angular caps from moved vertices after the motion window;
- require `broadphase_margin_rad > 0`;
- require `broadphase_margin_rad >= 10 * max_plate_motion_angle_rad` for the
  current resample window, or fall back to all-plate query for that run;
- keep a 50k all-plate equivalence gate before relying on broadphase-only scale
  characterization.

## Overlap Policy Before Process State

This is the load-bearing Milestone 2 decision:

Milestone 2 may not choose an overlap winner unless a source-grounded
process-state filter marks which overlapping triangles are invalid material
sources. Since the go-forward roadmap places full contact/process classification
after Milestone 2, the Milestone 2 primary path must treat nondegenerate
overlaps as deferred/unsupported write-back cases.

Allowed overlap handling in M2:

- count overlaps;
- attribute them to plate pairs;
- preserve them in diagnostics;
- fail fixtures that require overlap write-back;
- keep authority unchanged for blocked overlap samples in diagnostic-only runs.

Forbidden overlap handling in M2:

- choosing nearest/centroid/prior owner;
- silently picking the first hit;
- using old global owner identity;
- claiming paper-faithful overlap resolution without subduction/collision
  process-state filters.

This means M2 can exercise the carrier cycle for single-hit and divergent
zero-hit cases. Full convergent overlap material selection moves to the contact
and process milestone.

## q1/q2 And Ridge Identity

M2 q1/q2 must be geometric and continuous:

- q1 and q2 are boundary-derived positions from two different plate boundaries;
- qGamma is boundary midpoint/provenance for generated oceanic material;
- alpha uses geometric distances, not ownership history;
- q1/q2 records are diagnostic/source records, not new persistent global sample
  owners;
- ridge identity may be provisional for M2, but any field intended for later
  amplification must preserve enough identity to avoid re-inventing ridges from
  nearest pairs every step.

## Required Fixtures

Micro fixtures:

- no-motion resample no-op: hashes and material records remain stable;
- single-hit resample transfer: barycentric material equals source plate-local
  record;
- divergent gap fill: zero-hit samples generate q1/q2/qGamma records without
  fallback ownership;
- overlap-blocked sample: nondegenerate overlap is counted and not written by a
  resolver;
- repeated lifecycle: several no-overlap windows preserve conservation and
  field sharpness within thresholds; multi-window convergent motion is deferred
  until process filters exist;
- prior-owner contract tripwire: prior-owner/global-owner read counters must
  remain zero and the source path must be inspected; this fixture is not a live
  proof that a reachable prior-owner input was rejected.

Scale fixtures:

- 50k gate with all-plate broadphase equivalence;
- 100k characterization for miss/overlap scaling;
- 250k hard gate;
- optional 500k characterization, reported as pass/fail against a named budget.

## Required Metrics

M2 must report:

- fixture id, sample count, plate count, resample cadence, total motion window;
- valid single-hit count;
- divergent zero-hit count;
- q1/q2/qGamma generated count;
- nondegenerate overlap blocked count;
- unsupported overlap write attempts, required to be zero;
- prior-owner/global-owner reads, required to be zero;
- centroid/nearest/random resolver consumed count, required to be zero;
- remesh/rebuild counts;
- material conservation delta;
- field sharpness or total-variation delta across repeated remesh;
- per-plate and per-plate-pair miss/overlap attribution;
- replay authority/projection hashes;
- deferred overlap area/mass accounting;
- unassigned-triangle budget for rows that intentionally defer full topology;
- step kernel ms, full resample-cycle ms, total ms, and peak memory.

## Post-Review Hardening Amendment

External review of the Milestone 2 implementation found that the carrier cycle
was architecturally valid, but several report surfaces were too generous:

- scale material-conservation and total-variation deltas are characterization
  unless the config explicitly gates them;
- topology rows that block overlaps must use a bounded-unassigned budget rather
  than printing a generic pass;
- AABB build time must be visible in full-cycle timing because M2 rebuilds
  query trees every resample window;
- the prior-owner fixture is a contract/source-inspection tripwire, not a
  complete live negative control;
- M3 must add a hole-to-oceanic tripwire before any multi-window convergent run.

## Pass Gates

M2 passes only if:

- build passes;
- targeted automation passes;
- commandlet exits with `MILESTONE_2_PASS`;
- replay A/B authority and projection hashes match;
- no forbidden fallback counter is nonzero;
- overlap-blocked fixtures gate against inventing a primary overlap winner;
- divergent q1/q2 fixtures use continuous boundary evidence;
- repeated lifecycle fixtures preserve material conservation and field
  sharpness thresholds;
- 50k and 250k scale gates pass;
- 100k probe is reported;
- 500k is either attempted or explicitly named as not attempted, with reason.

## Stop Conditions

Stop and write an investigation note if:

- any sample write-back reads a previous global owner;
- a nondegenerate overlap is assigned without process-state filtering;
- q1/q2 comes from discrete endpoint picking or nearest-owner fallback;
- topology rebuild changes material totals beyond the predeclared tolerance;
- replay hashes diverge;
- 50k broadphase equivalence mismatches;
- 250k exceeds the declared hard budget;
- 500k remains above budget and the work is about to depend on 500k as a
  required scale.

## Implementation Steps

1. Add M2 configs and metrics only.
2. Add no-op and single-hit resample fixtures.
3. Add divergent q1/q2/qGamma fixture.
4. Add overlap-blocked negative fixture.
5. Add repeated lifecycle conservation and field-sharpness fixture.
6. Add scale probes and report tables.
7. Run build, automation, commandlet, remesh guard, commandlet gate review, and
   claim audit.
8. Write Milestone 2 closeout.
9. Stop for explicit user go/no-go before Milestone 3.

## Verdict

Milestone 2 is ready for user review as a plan. It is not approved for
implementation until the user explicitly says go.
