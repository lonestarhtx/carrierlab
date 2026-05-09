# Phase IIIE.6.9 Restore Paper-Literal Zero-Hit Generation

## Verdict

PASS / LIVE REMESH APPLY UNBLOCKED AT DEFAULT SCALE.

IIIE.6.9 demotes the IIIE.4 signed-separating-velocity veto from a stop condition to a diagnostic observation. This restores the thesis/CGF zero-hit route: after remesh ray filtering, a sample with no valid hit and a valid q1/q2 boundary pair generates divergent oceanic fields. Non-positive signed separation is now recorded through `bGeneratedWithNonPositiveSeparation` and aggregate `nonpos_sep` counters; it no longer invalidates the record by default.

This removes an uncited CarrierLab divergence rather than adding a new named lab policy. The IIIE named lab-policy count remains seven. The zGamma profile law, shared-boundary tie-breaks, nearest-hit tie-breaks, distance-tie fallback, majority assignment, triple-junction split, and rifting-pending handoff remain named lab choices for IIIE consolidation.

## Contract Change

| Surface | Before IIIE.6.9 | After IIIE.6.9 | Gate |
|---|---|---|---|
| IIIE.4 no-hit generation | `SignedSeparationVelocity <= 0` set `bNonSeparatingAnomaly` and stopped generation | Generation continues; `bGeneratedWithNonPositiveSeparation` records the condition | IIIE.4 `non-positive separation generation observability` row passes |
| Historical baseline | Always active veto | `bRestoreNonSeparatingAnomalyVeto` opt-in restores the old veto | IIIE.4 restored-veto fixture and IIIE.6.7 restored-veto scenario |
| Live apply invalid records | Non-positive separation could dominate invalid records | Default path reports zero invalids for that class | IIIE.6 live workbench smoke applies |
| Live actor visibility | Hold mode hid the remesh mutation | Apply mode reports `nonpos_sep` count in HUD/log/report | IIIE.6 default workbench smoke |

## Evidence

| Gate | Result | Evidence |
|---|---|---|
| Build | pass | `CarrierLabEditor` rebuilt successfully after code/report changes |
| IIIE.4 oceanic generation | pass | 11 fixture hashes matched; non-positive separation generated with hash `d8d7e6a7a434cbd0`; restored-veto baseline held with hash `bd36f88f2284a7bc` |
| IIIE.6.7/IIIE.6.9 step-60 diagnostic | pass | paper-literal run: selection `59079/40921/0`, invalid `0`, gen/nonpos/applied/rift `40921/19512/40921/0`, spatial hash `5d72d426aeb650d7` |
| Historical veto replay | pass | restored-veto run: invalid `19512`, gen/nonpos/applied/rift `21409/0/21409/0`, `divergent_gap_nonseparating = 19512` |
| Same-seed replay | pass | step-60 paper-literal replay kept diagnosis hash `228a012b2f8a4975` and spatial hash `5d72d426aeb650d7` |
| IIIE.6 live workbench smoke | pass | default `100000/40` at step 20 applied, events `0->1`, gen/apply/rift/nonpos/invalid `32727/32727/0/16304/0`, mode `phase_iii_e6_live_apply ... nonpos_sep=16304 ...` |
| IIIE.3 selector regression | pass | selector commandlet exited successfully after the zero-hit change |

## Interpretation

The previous `divergent_gap_nonseparating` blocker was not a multi-hit, process-state, or topology failure. It was an extra CarrierLab veto applied after the paper's zero-hit q1/q2 route had already found a valid boundary pair. IIIE.6.8 research classified the paper as silent on a signed-separation veto, so preserving that veto as default behavior would over-constrain the clean-room reproduction.

IIIE.6.9 therefore restores paper-literal zero-hit generation while keeping the condition visible. The default live actor now mutates state when the workbench remesh button or auto-cadence path reaches the IIIE.6 chain. The old behavior remains reproducible only through the explicit `bRestoreNonSeparatingAnomalyVeto` diagnostic/baseline switch.

## Remaining Caveats

- This does not claim full IIIE paper fidelity. The zGamma profile remains a geophysics-derived lab extension, and several multi-hit/topology rules remain named lab policies.
- This does not optimize remesh. The IIIE.6.7 timing still shows about `313s` in the continuous boundary-pair query for the step-60 diagnostic. IIIE.6.10 should target that cost before treating live cadence as usable/playable.
- This does not implement IIIF rifting. Continental divergent samples still route through the approved rifting-pending handoff.

## Next

Proceed to IIIE.6.10 performance containment for the continuous boundary-pair query. After that, IIIE consolidation should disclose the seven named lab policies, explicitly record that IIIE.6.9 removed an uncited signed-separation veto, rerun the relevant IIIE gates, visually validate the live actor mutation path, and measure integrated cost.
