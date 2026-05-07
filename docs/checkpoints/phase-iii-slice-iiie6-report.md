# Phase IIIE.6 Remesh Ledger Reframe And Cadence Wire-Up

Verdict: PASS / IIIE CONSOLIDATION UNBLOCKED; NAMED LAB-POLICY HOLDS CARRIED. This slice wires the IIIE.3 divergent route, IIIE.4 oceanic generation, and IIIE.5 topology rebuild/reset helpers into a focused remesh-event audit, adds ledger lines for new oceanic creation and ridge overwrite, and closes the post-rebuild IIIB tracking discontinuity. It does not add optimization, resolve triple-junction policy, claim zGamma's profile law is paper-sourced, or retire legacy comparison code.

## Scope

- The event audit consumes only IIIE.3 routes that are legal for IIIE.4: no-hit divergent gaps and filter-exhausted divergent gaps. Unresolved multi-hit selection classes remain stop conditions and are not routed to ocean generation.
- Generated oceanic vertices are fed into the IIIE.5 duplicate/re-index/re-compact helper, so selection, gap fill, topology rebuild, and process reset are exercised in one bounded cadence gate.
- The material ledger is reframed into two explicit generated-ocean lines: `new oceanic creation` when the pre-remesh continental fraction is effectively zero, and `overwritten by ridge generation` when generated oceanic crust would replace pre-existing continental material.
- `overwritten by ridge generation` is a hold/anomaly line, not a hidden correction. IIIE consolidation must decide whether this remains an approved lab policy, becomes a stop condition in production cadence, or receives paper-cited handling.
- The post-rebuild IIIB gate seeds convergence tracking after IIIE.5 topology rebuild/reset, then checks IIIB active lists, distance records, subduction matrix evidence, neighbor propagation, and hash closure on the rebuilt local topology.

## Gates

| Gate | Result | Evidence |
|---|---:|---|
| No-hit live remesh event ledger | pass | selection `no-hit divergent gap`, raw/post-filter `0/0`, generated `1`, q1/q2 `0/1`, new oceanic `1` (`1.000` sample-eq), overwritten `0` (`0.000` continental fraction), ledger reconciles `1`, reset `0->1`, policy/prior/projection `0/0/0`, q/zGamma `1/1`, geophysics/paper zGamma `1/0`, topology `f4e282af487e88e7`, event `fe3f48c75fc1381a`. |
| Filter-exhausted continental overwrite hold | hold | selection `divergent gap after filtering`, raw/post-filter `2/0`, generated `1`, q1/q2 `0/1`, new oceanic `0` (`0.000` sample-eq), overwritten `1` (`1.000` continental fraction), ledger reconciles `1`, reset `0->1`, policy/prior/projection `0/0/0`, q/zGamma `1/1`, geophysics/paper zGamma `1/0`, topology `f9a8e8b58238b830`, event `486f11b2febb008f`. |
| Post-rebuild IIIB tracking gate | pass | topology `f23cd4346fd0480b`, reset `0->1`, seed plate/local `0/170`, other `1`, active `2`, distances `2`, matrix pairs `1`, accepted positive `1`, propagation seed/added `1/1`, closure reset `1`, hash `219f4be735724106`. |
| Same-seed remesh-event replay | pass | Event hashes `fe3f48c75fc1381a` and `fe3f48c75fc1381a`. |

## Contract Table

| Paper / IIIE.1 requirement | CarrierLab support now | Remaining obligation | Gate |
|---|---|---|---|
| Zero valid ray hits become divergent gap fill | No-hit IIIE.3 records route through IIIE.4 q1/q2/qGamma and then IIIE.5 rebuild/reset | Production cadence still needs scheduling around the natural remesh timestep | No-hit live remesh event ledger |
| Filter-exhausted hits become divergent gap fill only after process-state filtering | Filter-exhausted records route through the same oceanic generation path and preserve filter provenance via the IIIE.3 selection class | Collision/remesh same-step ordering remains the IIIE.1 convention: collision/suture authorization before remesh filtering | Filter-exhausted overwrite hold |
| New oceanic crust must be ledgered distinctly | IIIE.6 records `new oceanic creation` as a generated-ocean ledger line when pre-remesh continental material is absent | Convert this audit line into production material-ledger accounting when full cadence mutates live state | Ledger reconciliation columns |
| Ridge generation must not silently overwrite continental material | IIIE.6 records `overwritten by ridge generation` separately and marks it hold/anomaly evidence | Consolidation must approve, forbid, or paper-cite this behavior before broad validation claims | Overwrite hold fixture |
| Plate-local topology rebuild/reset must be the event continuation | IIIE.6 feeds generated records into the IIIE.5 duplicate/re-index/re-compact helper and observes reset in the same event gate | Keep Stage 1.5 recovery out of the primary path | Topology hash and reset columns |
| IIIB tracking must work after rebuild | A post-rebuild actor seeds IIIB tracking from rebuilt local topology and checks active lists, distances, matrix evidence, propagation, and hash closure | Consolidation should still rerun the `CarrierLabPhaseIIID7` computed-vs-expected regression separately; this local gate only closes the topology boundary discontinuity | Post-rebuild IIIB tracking gate |

## Forbidden Policy Checks

| Forbidden or held policy | IIIE.6 status |
|---|---|
| Prior global owner/fraction fallback | Selection, generation, and topology counters remain zero. |
| Projection-derived ownership authority | Topology projection-authority counter remains zero. |
| Uncited remesh winner | Policy-winner counters remain zero; no multi-hit route is consumed. |
| Stage 1.5 recovery/backfill/retention/hysteresis/anchoring | Not called by the event audit; IIIE.5 rebuild remains the authority path. |
| Unresolved multi-hit ridge generation | Not routed; IIIE.4 receives only no-hit and filter-exhausted divergent classes. |
| zGamma paper-fidelity overclaim | Generated records preserve `bUsedZGammaGeophysicsDerivedProfile = true` and `bPaperFaithfulZGammaProfile = false`. |
| Silent continental overwrite by divergent ridge generation | Moved into an explicit hold/anomaly ledger line. |

## Open Decisions

| Decision | Status | Rationale |
|---|---|---|
| zGamma profile law | Deferred / named lab extension | Current sqrt-distance profile is geophysics-derived and realistic, but the paper/thesis do not provide a closed-form zGamma equation. |
| Two-of-three mixed triangle majority | Approved CarrierLab lab policy | IIIE.5 exposes and gates the deterministic majority rule: if exactly two global-TDS vertices assign to one plate, that plate owns the rebuilt triangle. This is approved only as disclosed lab policy, not as paper text. |
| One-one-one triple-junction topology | Deferred / hold | IIIE.5 leaves these unresolved rather than inventing authority. |
| Continental overwrite by divergent ridge generation | Deferred / hold | IIIE.6 makes the ledger line visible and non-silent; production cadence policy remains a consolidation decision. |

## Stop Conditions For IIIE Consolidation+

- Stop if unresolved multi-hit IIIE.3 classes are routed into IIIE.4 generation.
- Stop if `overwritten by ridge generation` is merged into `new oceanic creation` or hidden in aggregate material accounting.
- Stop if Stage 1.5 owner/projection/recovery behavior becomes primary IIIE remesh authority.
- Stop if post-rebuild IIIB tracking cannot seed active lists, distance records, matrix evidence, propagation, and closure from rebuilt plate-local topology.
- Stop if reports claim paper-faithful zGamma while generated records still report `bUsedZGammaGeophysicsDerivedProfile = true` and `bPaperFaithfulZGammaProfile = false`.
- Stop if the majority rule is described as paper-faithful rather than approved lab policy, or if any triple-junction topology policy is used without explicit approval and gates.

## Next Slice Boundary

Next is IIIE consolidation: disclose the named lab choices (geophysics-derived zGamma, approved two-of-three majority assignment, triple-junction handling, and ridge-overwrite handling), rerun the relevant IIIE gates, keep the inherited IIIB/IIID signature trail visible, and measure the integrated paper Table 2 cost ratio. Do not start IIIF rifting, IIIG per-step elevation evolution, or IIIH long-horizon validation until consolidation explicitly clears.

Metrics: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIE6/phase-iii-slice-iiie6-metrics.jsonl`.
