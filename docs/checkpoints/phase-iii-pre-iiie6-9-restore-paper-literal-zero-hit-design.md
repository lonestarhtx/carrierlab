# Pre-IIIE.6.9 Design — Restore Paper-Literal Zero-Hit Divergent Generation

## Status

**DESIGN READY.** This slice is a policy reversal: it removes an
undisclosed CarrierLab-invented constraint and restores paper-literal
behavior. It is NOT a new lab policy. The remaining observability
fields are diagnostic, not authoritative.

## Background

IIIE.6.7 (`30ffd03`) diagnosed that, at the step-60 default 100k/40
seed-42 scenario, 19,512 of 40,921 zero-hit divergent gap samples are
flagged invalid as `divergent_gap_nonseparating`. IIIE.4's
signed-separating-velocity check is producing these invalids and
blocking live remesh.

IIIE.6.8 research (`docs/checkpoints/phase-iii-slice-iiie6-8-non-separating-boundary-research.md`)
established with verbatim page-image citations:

- Thesis §3.3.2.3 (`cc5c6807-079`, page 68): "aucune intersection
  valide" → divergence zone, q1/q2 = nearest geometric plate-boundary
  pair, ocean generation proceeds. **No signed-separation eligibility
  test specified.**
- Thesis §3.3.2.1 (`cc5c6807-077`, page 66): conceptual blend formula;
  silent on motion-based eligibility.
- CGF §4.3 (`aa42e52c-07`): same conceptual framing; silent on
  separation test.
- Thesis §3.1.2 (`cc5c6807-054`): mentions "décrochement
  transformantes" as a third tectonic mode set aside from modeled
  relief; no §3.3.2.3 resampling rule given for transform regions.

Pro reviewer and Codex both independently flagged that the IIIE.4
signed-separation check is CarrierLab-invented discipline, not
thesis-cited, and is now blocking the paper-literal remesh path.

## Framing

This slice is **NOT** introducing a new lab policy.

This slice IS **removing an undisclosed CarrierLab divergence from
the paper.** The IIIE.4 signed-separation check was added at IIIE.4
landing (commit `14e4ab7`) framed as discipline ("non-positive
velocity is anomaly, not fallback"), and propagated through IIIE.5 /
IIIE.6 / IIIE.6.4 reviews without verification. It is functionally a
veto on paper-faithful behavior.

The project's discipline is "don't add undisclosed divergences from
the paper." Removing the veto returns us TOWARD paper-faithfulness,
not away from it. **This is course correction, not new policy.**

## Decision

Demote the IIIE.4 signed-separating-velocity check from stop
condition to diagnostic observation.

- Before: zero-hit divergent gap with non-positive signed separation
  → invalid record → live remesh held
- After: zero-hit divergent gap → generate ocean per §3.3.2.3 literal
  text using geometric q1/q2/qGamma; record signed-separation value
  as diagnostic only

## Implementation packet for Codex

### Files to touch

| File | Change |
|---|---|
| `Source/CarrierLab/Private/CarrierLabVisualizationActor.cpp` | In `PopulatePhaseIIIE4OceanicRecord` (around line 2407): remove the early-return that flags `bNonSeparatingAnomaly = true` on `SignedSeparationVelocity <= 0`. Generate ocean unconditionally for zero-hit samples that have a valid q1/q2 boundary pair. Preserve `SignedSeparationVelocity` as a recorded field. Add `bGeneratedWithNonPositiveSeparation = (SignedSeparationVelocity <= 0)` flag. |
| `Source/CarrierLab/Public/CarrierLabVisualizationActor.h` | Add `bGeneratedWithNonPositiveSeparation` field to oceanic generation record. Add aggregate counter `GeneratedWithNonPositiveSeparationCount` plus min/median/max separation magnitudes for non-positive cases. |
| `Source/CarrierLab/Private/CarrierLabPhaseIIIE4Commandlet.cpp` | Update gate: "non-separating q1/q2 anomaly" gate is replaced with "non-positive separation generation observability" gate. The fixture that previously asserted held-as-anomaly now asserts generated-with-counter-incremented. |
| `Source/CarrierLab/Private/CarrierLabPhaseIIIE6Commandlet.cpp` | The default 100k/40 live-cadence gate now expects 0 invalid records and successful live application. The gate that previously expected `phase_iii_e6_live_hold_invalid_records_*` mode now expects `phase_iii_e6_live_apply` mode. |
| `Source/CarrierLab/Private/CarrierLabPhaseIIIE67ApplyPathInvalidRecordsCommandlet.cpp` | The `divergent_gap_nonseparating` invalid-record class is demoted to the restored-veto baseline only. The default diagnostic still records signed separation per generated record, but it is no longer an invalidation reason. |
| `docs/checkpoints/phase-iii-slice-iiie6-9-restore-paper-literal-zero-hit.md` | New slice report. |

### Hook point

`PopulatePhaseIIIE4OceanicRecord` in
`CarrierLabVisualizationActor.cpp`. The current code path early-returns
on `SignedSeparationVelocity <= 0` setting `bNonSeparatingAnomaly`.
Remove that early return. Continue to ocean generation. Set the new
observability flag.

### New audit fields

**Per-record:**
- `bGeneratedWithNonPositiveSeparation` (bool): true if record was
  generated despite SignedSeparationVelocity <= 0
- `SignedSeparationVelocity` (double, already recorded): preserved

**Aggregate:**
- `GeneratedWithNonPositiveSeparationCount`: total across cadence
- `NonPositiveSeparationMinMagnitude`: smallest |v| among
  non-positive cases
- `NonPositiveSeparationMedianMagnitude`: median |v|
- `NonPositiveSeparationMaxMagnitude`: largest |v|
- `NonPositiveSeparationSpatialHash`: deterministic hash of the
  non-positive samples' positions for replay verification

### Demoted audit fields

- `bNonSeparatingAnomaly` remains available only for legacy opt-out /
  baseline reproduction when `bRestoreNonSeparatingAnomalyVeto = true`.
  Under default behavior it is not set for non-positive separation.
- `divergent_gap_nonseparating` remains a historical diagnostic reason
  for the restored-veto baseline. Under default behavior it is no
  longer an invalidation reason.

### Acceptance criteria

1. **Default 100k/40 manual and auto cadence have zero invalid records.**
   The `divergent_gap_nonseparating` class is eliminated under default
   behavior. Counts are reported per scenario rather than copied from
   the IIIE.6.7 step-60 diagnostic.

2. **Live remesh advances at default scale.** Manual remesh button
   produces `events 0→1`, projection/state/crust hashes change, mode
   string is `phase_iii_e6_live_apply` (NOT
   `phase_iii_e6_live_hold_invalid_records_*`). Auto cadence at step
   32 produces the same.

3. **Same-seed determinism preserved.** Two replays of the live
   remesh produce byte-identical hashes.

4. **Observability fields populate.**
   `GeneratedWithNonPositiveSeparationCount` is non-zero (expected
   ~19,512 ± motion-step variance at default scale). The min/median/
   max magnitudes are reported. Spatial hash is deterministic across
   replays.

5. **IIIE.6.7 / IIIE.4-veto baseline reproducible via opt-out flag.**
   New flag `bRestoreNonSeparatingAnomalyVeto` defaulting false. When
   set true, restores the IIIE.4 pre-IIIE.6.9 behavior (held as
   anomaly). The IIIE.6.7 step-60 baseline distribution (19,512
   invalid records) is reproducible in the diagnostic commandlet.

6. **Forbidden-policy counters stay zero.** `bUsedPolicyWinner = 0`,
   `bUsedPriorOwnerFallback = 0`, projection-authority counter zero.
   The new observability flag `bGeneratedWithNonPositiveSeparation`
   is NOT a forbidden-policy flag — it's diagnostic.

7. **No regression in pre-existing IIIE gates.** All prior IIIE
   slice commandlets pass (IIIE.3, IIIE.5, IIIE.6.3, IIIE.6.5,
   IIIE.6.6).

### Stop conditions

- Stop if removing the veto introduces any new forbidden policy
  invocation. The check `bUsedPolicyWinner / bUsedPriorOwnerFallback`
  must remain zero.
- Stop if the live remesh apply path produces NEW invalid-record
  classes (not the demoted historical `divergent_gap_nonseparating`
  baseline reason). If new
  classes appear, surface them as a separate diagnostic; this slice
  only addresses the non-separating class.
- Stop if observability reveals the non-positive-separation
  generation clusters at suspiciously specific geometric features
  (e.g., all 19,512 samples are at exactly one boundary edge). That
  would suggest a real bug being papered over rather than paper-
  literal generation.
- Stop if same-seed determinism breaks across replays.

## Performance note

This slice removes a check; it does not address the 5+ minute
boundary-pair query cost in record building. **Performance
optimization is IIIE.6.10 territory and remains owed.** The boundary-
pair query for all 40,921 divergent gap samples in the step-60
diagnostic (now all generating ocean instead of 21,409 generating +
19,512 holding) means the perf
cost may even slightly increase post-IIIE.6.9. This is acceptable —
optimize after semantics are settled.

## Disclosure language for IIIE.6.9 slice report

```
IIIE.4's signed-separating-velocity check, originally framed as
paper-faithful discipline, was confirmed by IIIE.6.8 research and
independent Pro/Codex review to be CarrierLab-invented — not
authorized by thesis §3.3.2.3 or CGF §4.3. The check produced 19,512
invalid records in the IIIE.6.7 step-60 default 100k/40 diagnostic,
blocking live remesh. IIIE.6.9 demotes the check from stop condition to diagnostic
observation, restoring paper-literal zero-hit divergent generation as
specified in thesis §3.3.2.3 ("aucune intersection valide" →
divergence → ocean generation from nearest geometric q1/q2). The
signed-separation value remains observable per record (counter
`GeneratedWithNonPositiveSeparationCount` plus magnitude statistics)
for future analysis but does not gate generation.

This slice does NOT introduce a new IIIE lab policy. It REMOVES an
undisclosed CarrierLab-invented divergence from the paper. The IIIE
named-lab-policy count remains at seven (zGamma sqrt-subsidence,
2-of-3 majority, triple-junction centroid-split, continental
overwrite → rifting-pending, shared-boundary tie-break, nearest-hit
tie-break, distance-tie fallback). The IIIE.4 signed-separation
check is removed from the disclosed-policy list rather than added.
```

## Stop conditions for IIIE consolidation

- Stop if IIIE consolidation report claims the signed-separation
  check was paper-faithful. It wasn't.
- Stop if IIIE consolidation increments the lab-policy count to
  eight (or higher) by listing this slice's outcome as a new policy.
  It's removal of an undisclosed policy, not addition.

## Open questions

1. **What does the visual look like with non-positive separation
   generation enabled?** Unknown until user runs the actor. The
   spatial distribution data + min/median/max magnitudes should give
   a quantitative read; the user's editor inspection answers whether
   the visible result is acceptable.

2. **Will any of the 19,512 cases produce extreme depth values that
   look obviously wrong?** The geophysics zGamma profile maps
   distance to depth; non-positive-separation cases still have valid
   q1/q2 distance, so depth values fall within the same -1 to -6 km
   range as separating cases. Magnitude shouldn't be pathological.
   But spatial concentration could produce visible artifacts at
   transform-equivalent regions.

3. **Are there other CarrierLab-invented checks still hiding?** This
   experience suggests an audit-pass-by-pro for all IIIE-X discipline
   checks, asking "is this paper-cited or invented?" Probably worth a
   methodology slice after IIIE consolidation.

## Recommendation

**Ready for Codex execution as designed.** Small slice (~50-100 lines
of code change, plus updates to commandlets and reports). Removes a
check, adds observability counters. The discipline pattern is
preserved (audit-by-construction; observability not authority;
forbidden-policy counters preserved at zero).

After this slice lands:
- Live remesh visibly advances at default 100k/40 (the user-visible
  milestone they've been waiting for)
- IIIE.6.10 perf optimization can proceed against settled semantics
- IIIE consolidation can begin (seven named lab policies disclosed,
  Stage 1.5 retired, integrated cost ratio measured, IIIE consolidation
  report drafted)

---

**Summary for independent review:** IIIE.6.9 removes the CarrierLab-
invented signed-separating-velocity veto introduced at IIIE.4 (commit
`14e4ab7`) and confirmed by IIIE.6.8 research as not paper-cited. The
slice restores paper-literal §3.3.2.3 zero-hit-to-divergent-generation
behavior, eliminates the step-60 19,512-invalid-records blocker,
unblocks live remesh at default 100k/40 scale, and preserves
diagnostic observability (signed-separation value, generation count,
spatial distribution) without authoritative gating. The IIIE
named-lab-policy count stays at seven; this is removal of an
undisclosed divergence, not addition of a new policy.
