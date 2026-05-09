---
name: carrierlab-iiie-remesh-triage
description: Diagnose why CarrierLab Phase IIIE live remesh or the editor remesh button appears to do nothing. Use when the actor does not visibly remesh, event/hash counts do not change, HUD mode strings report holds, or the user asks what currently blocks IIIE live cadence.
---

# CarrierLab IIIE Remesh Triage

## Goal

Answer one question quickly: which IIIE surface held, and what evidence proves it?

## Triage Order

1. **Cadence due?**
   - Check HUD fields: `Step / Myr`, `Next Resample`, auto-remesh toggle.
   - Manual `Run IIIE.6 Remesh` may fire anytime; automatic cadence waits for the speed-derived target.

2. **Selection closed?**
   - Look for unresolved multi-hit counts in current reports/logs.
   - If selection holds appear, inspect IIIE.6.1 / IIIE.6.2 / IIIE.6.3 / IIIE.6.5 / IIIE.6.6 reports by class.
   - Current expected post-IIIE.6.6 default: selection accounts for all samples with `0` unresolved selection holds.

3. **Apply-record builder held?**
   - Check `PhaseIIIELastInvalidRecordCount` and `LastRemeshMode`.
   - `phase_iii_e6_live_hold_invalid_records_*` means selection succeeded but apply-record validation refused mutation.
   - IIIE.6.7 classified this as `divergent_gap_nonseparating` in the current default diagnostic.

4. **Mutation actually happened?**
   - Event count should increment.
   - Crust/state/projection hash should change.
   - Mode should say apply/success, not hold.
   - If event count and hashes are unchanged, do not describe the remesh as successful.

5. **Stage 1.5 contamination?**
   - The legacy Stage 1.5 button/path is comparison-only.
   - Do not treat Stage 1.5 visual changes as evidence that IIIE remesh worked.

## Evidence Sources

- `docs/checkpoints/phase-iii-slice-iiie6-7-apply-path-invalid-records-diagnosis.md`
- latest IIIE.6 reports in `docs/checkpoints/`
- actor HUD mode string and counters
- commandlet logs under `Saved/Logs/`
- JSONL metrics under `Saved/CarrierLab/PhaseIII/`

## Answer Shape

Use this compact shape:

```text
Current blocker: <surface>
What succeeded: <selection/cadence/query/etc.>
What held: <exact HUD/report mode or count>
Why it is not visible: <event/hash unchanged reason>
Next slice: <research/design/implementation target>
```

## Guardrails

- Do not say "remesh did nothing" without naming the hold surface.
- Do not say "paper-faithful" when the active behavior is a named lab policy.
- Do not propose fallback ownership, prior-owner retention, or Stage 1.5 promotion as a fix.
- If the symptom is expensive/hung commandlets, use `carrierlab-long-commandlet-monitor`.
