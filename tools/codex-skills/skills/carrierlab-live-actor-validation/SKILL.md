---
name: carrierlab-live-actor-validation
description: Validate what CarrierLab's live editor actor is actually showing after Phase III/IIIE changes. Use when the user asks what they should see, whether the remesh button worked, whether visuals prove the system, or when comparing HUD counters, event counts, hashes, layers, and map/contact-sheet evidence.
---

# CarrierLab Live Actor Validation

## Principle

The actor is a diagnostic surface, not authority. Trust state counters, event/hash changes, commandlet reports, and checkpoint gates first; use visuals to localize and sanity-check.

## Minimum Checks

When the user asks whether the live actor worked:

1. **HUD mode string**
   - Apply/success mode means mutation may have happened.
   - Hold mode means no mutation should be claimed.

2. **Events**
   - Event count must increment for a live remesh mutation.
   - If events stay unchanged, explain which hold blocked mutation.

3. **Hashes**
   - State/crust/projection hashes should change after successful remesh.
   - Unchanged hash plus hold mode means the actor is honestly refusing mutation.

4. **Layer**
   - `PlateId`: topology/plate assignment visibility.
   - `PhaseIIISummary`: process summary, not proof of remesh success.
   - `OceanicAge` / elevation: only meaningful after generation applied.

5. **Cadence**
   - Manual remesh can fire any step.
   - Automatic remesh depends on speed-derived cadence and should not reset on held events.

## Visual Evidence Rules

- A screenshot can show symptoms: chaotic boundaries, unchanged plates, blank layer, held UI.
- A screenshot cannot prove paper-faithfulness.
- Contact sheets are diagnostic evidence, not acceptance gates unless the slice defines that.
- Always pair visual claims with mode string, event count, and relevant commandlet/report.

## Useful Answer Shape

```text
What you are seeing: <visual symptom>
What the actor state says: <mode/events/hash/layer>
Whether remesh applied: yes/no/unknown
Why: <gate or hold>
What to inspect next: <layer/report/commandlet>
```

## Stop Conditions

- Stop if the UI implies success but event count/hash did not change.
- Stop if the visual path uses Stage 1.5 or legacy policy while being described as IIIE.
- Stop if a layer is blank/uniform and the explanation depends only on visual interpretation.
- Stop if manual and automatic remesh paths disagree without a report explaining why.
