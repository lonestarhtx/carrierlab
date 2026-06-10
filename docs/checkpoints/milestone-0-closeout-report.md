# CarrierLab Milestone 0 Closeout Report

Date: 2026-06-09

Branch: `codex/v2-0-carrier`

Base commit: `98d8e1b84d2cf4334b67b265ea4b0ff1a01ffe07`

Status: verified and accepted by user go/no-go on 2026-06-09.

## Decision

Question: do we have enough carrier-authority foundation evidence to stop the
foundation bundle and prepare the Milestone 1 entry packet?

Answer: yes, with limits.

Milestone 0 has enough evidence to close as a foundation checkpoint. The current
clean-room carrier kernel is gated through deterministic plate-local
authority, move and inspect that authority through numeric fixtures, filter and
resample from moved plate-local triangles, perform q1/q2 gap fill for zero-valid
samples, rebuild plate-local topology, and reset process marks without using
persistent global sample ownership, ownership repair, prior-owner fallback, or
visual output as authority.

This is not approval to implement the next behavior. It is approval-ready only
for writing the Milestone 1 entry packet.

Recommended verdict: `READY_FOR_USER_GO_NO_GO_TO_PREPARE_MILESTONE_1_ENTRY_PACKET`.

## Scope Consolidated

Milestone 0 consolidates the current working bundle that older files call V2
Stage 0 through V2 Stage 5, plus the foundation stepper actor. Those historical
names remain in commandlets, test names, and generated reports for traceability,
but the go-forward structure is the milestone roadmap.

In-scope evidence:

- deterministic global substrate used as query and resampling target;
- duplicated plate-local triangulations used as moving carrier authority;
- material state carried on plate-local samples;
- center-ray projection from current plate-local authority;
- rigid motion checked by independent analytic oracle;
- contact and process state recorded before remesh;
- filtered global sampling from moved plate-local triangles;
- q1/q2/qGamma gap-fill provenance for zero-valid samples;
- topology rebuild from resampled global assignments;
- process-mark reset after rebuild;
- replay determinism, forbidden fallback counters, and performance readouts;
- optional editor diagnostic actor over existing numeric evidence.

Out-of-scope behavior:

- elevation, erosion, uplift, slab pull, rifting, and terrain beauty;
- visual output as a pass/fail gate;
- persistent global sample ownership as tectonic authority;
- ownership repair, retention, hysteresis, anchoring, or prior-owner fallback;
- centroid, random, nearest, or prior-owner resolver policies in the
  paper-faithful path;
- full repeated carrier lifecycle/cadence;
- 500k performance gate.

## Evidence Index

| Evidence | Report | Closeout reading |
|---|---|---|
| Cold-start carrier | `docs/checkpoints/v2-stage-0-report.md` | Positive/negative fixtures pass, 50k gate passes, 250k characterization passes, global-owner reads are zero. |
| Rigid motion | `docs/checkpoints/v2-stage-1-report.md` | Motion oracle passes, material attachments are preserved, raw gaps/overlaps are counted and not repaired. |
| Contact dry-run | `docs/checkpoints/v2-stage-2-report.md` | Moved-geometry contacts and polarity candidates are recorded without material mutation, remesh, gap fill, or owner-label evidence. |
| Process mutation ledger | `docs/checkpoints/v2-stage-3-report.md` | Subduction/collision candidate process state is recorded with provenance, replay hashes pass, and topology/material mutation remains forbidden. |
| Filtered global sampling | `docs/checkpoints/v2-stage-4-report.md` | Every global TDS vertex is sampled against moved plate-local triangles; subducting/colliding marks are filtered before source selection; zero-valid samples are deferred. |
| Q1/q2 gap fill and rebuild | `docs/checkpoints/v2-stage-5-report.md` | Continuous q1/q2 pairs from different plates are required; generated oceanic material occurs only through gap fill; topology rebuild and process reset pass. |
| Diagnostic actor | `docs/checkpoints/v2-foundation-stepper-actor-report.md` | Actor is available for later inspection but is not milestone authority; visual inspection is deferred. |

## Numeric Gate Summary

| Gate | Required for closeout | Current evidence |
|---|---|---|
| Micro fixtures | pass | Pass across the consolidated Stage 0-5 reports. |
| 50k scale | pass | Pass across the consolidated Stage 0-5 reports. |
| 250k scale | characterization | Attempted and passing across the consolidated Stage 0-5 reports. |
| 500k scale | not required | Not attempted for Milestone 0 closeout. |
| Replay determinism | pass | Replay A/B hashes pass in each historical report. |
| Forbidden fallback counters | zero | Prior-owner fallback, ownership repair, retention/hysteresis, centroid, random, nearest, and terrain-beauty counters report zero where applicable. |
| Viewport inspection | optional diagnostic | Deferred; the actor is not a pass gate. |

## Performance Readout

Performance is sufficient for Milestone 0 foundation closeout at the required
50k scale and characterized at 250k, but it is not yet proof that the final
editor tool will meet paper-scale interactive goals.

The heaviest current 250k readout is the q1/q2 gap-fill and topology rebuild
path in `v2-stage-5-report.md`, which reports total time `11406.266 ms` and
peak memory `2031.094 MB`. That is acceptable for a closeout characterization,
not a final target.

Milestone 1 must define fresh timing budgets and stop conditions before adding
new behavior.

## Lab Policies To Carry Forward

The closeout is acceptable only because named lab policies are explicit instead
of hidden:

- boundary-only degenerate multi-hits are classified separately;
- micro fixtures may use exhaustive scans for correctness diagnostics;
- scale motion/contact readouts use source-adjacent candidates where the report
  names them as a lab policy, not final broadphase authority;
- fixture material profiles are test input policy, not global owner authority;
- mixed-vertex global triangle partition after remesh is deterministic lab
  policy because the paper/thesis do not specify that exact implementation
  detail.

These policies may remain as documented test scaffolding or implementation
details, but future entry packets must say whether each one is still allowed.

## Known Limits

- The carrier is not yet validated as a repeated multi-step lifecycle.
- The current reports gate foundation mechanics and one-shot remesh components,
  not long-run coherence.
- The current editor actor explains evidence; it does not validate tectonic
  behavior.
- Continental persistence still needs future terrane/material transfer design.
- Terrain plausibility is entirely unproven.
- 500k remains the aspirational paper-scale target and has not been attempted in
  this closeout.

## Verification

Final closeout verification was run on 2026-06-09.

| Gate | Command | Result |
|---|---|---|
| CarrierLabEditor build | `powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\Users\Michael\.codex\skills\carrierlab-build\scripts\Run-CarrierLabEditorBuild.ps1` | pass; target up to date |
| V2 automation filter | `UnrealEditor-Cmd.exe CarrierLab.uproject -NullRHI -NoSound -Unattended -nop4 -nosplash -ExecCmds="Automation RunTests CarrierLab.V2" -TestExit="Automation Test Queue Empty" -log` | pass; 7 tests found and 7 succeeded |
| Claim audit | `Invoke-CarrierLabClaimAudit.ps1 -ChangedOnly` | pass; high=0, medium=0, info=27 |
| Whitespace check | `git diff --check` | pass; no whitespace errors |

Automation tests passed:

- `CarrierLab.V2.FoundationStepper.Snapshot`
- `CarrierLab.V2.Stage0.EntryFixtures`
- `CarrierLab.V2.Stage1.RigidMotionFixtures`
- `CarrierLab.V2.Stage2.ContactDryRunFixtures`
- `CarrierLab.V2.Stage3.ProcessMutationFixtures`
- `CarrierLab.V2.Stage4.FilteredGlobalSamplingFixtures`
- `CarrierLab.V2.Stage5.Q1Q2GapFillRebuildFixtures`

## Closeout Verdict

Milestone 0 is closed by user go/no-go.

Closeout does not authorize implementation of Milestone 1. The authorized next
work is the Milestone 1 entry packet: fixtures, evidence gates, performance
budgets, stop conditions, and report shape for motion.
