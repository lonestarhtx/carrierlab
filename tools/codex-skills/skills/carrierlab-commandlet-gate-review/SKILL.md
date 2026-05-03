---
name: carrierlab-commandlet-gate-review
description: Review CarrierLab commandlet gates for real enforcement rather than report wording. Use when adding or reviewing checkpoint commandlets, consolidation commandlets, replay signatures, oracle residuals, pass/fail predicates, or GPT/Pro findings about weak gates.
---

# CarrierLab Commandlet Gate Review

## Quick Scan

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File '<skill-dir>\scripts\Find-CarrierLabGateRisks.ps1'
```

For one commandlet:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File '<skill-dir>\scripts\Find-CarrierLabGateRisks.ps1' -Path 'Source/CarrierLab/Private/CarrierLabPhaseIIICConsolidationCommandlet.cpp'
```

## What To Verify Manually

For every claimed gate:

1. Locate the report row.
2. Locate the predicate that computes that row.
3. Locate the final `bPass` / return path.
4. Confirm the row predicate is included in final pass/fail.
5. Confirm expected tokens are compared to recomputed values, not merely
   non-empty or self-consistent.
6. Confirm replay A/B equality uses independent reruns, not reused objects.
7. Confirm formula oracles recompute from raw fixture/config fields, not from
   implementation-produced counters.

## Known Risk Patterns

- `Expected...` token checked with `FCString::Strlen(...) > 0`.
- `bMetricsHashMatchesComputed` used as a substitute for an expected-token
  comparison.
- `BuildReport()` has a gate variable that is absent from final `bPass`.
- Report says "independent" but code hashes only one aggregate implementation
  value.
- Negative control seeds synthetic evidence but report describes natural
  geometry.

Treat scanner hits as prompts to read code; they are not automatic findings.
