---
name: carrierlab-remesh-guard
description: Guard CarrierLab remesh/resampling changes against paper-contract drift. Use before or during Stage 1.5, Phase IIIE, natural resampling, q1/q2, remesh, gap-fill, or multi-hit policy work to scan diffs for forbidden prior-owner fallback, centroid/random primary-path promotion, missing subducting/colliding filters, discrete q1/q2 authority, or process-state reset gaps.
---

# CarrierLab Remesh Guard

## Quick Scan

Run the helper from the repo root:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File '<skill-dir>\scripts\Invoke-CarrierLabRemeshGuard.ps1'
```

For staged changes only:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File '<skill-dir>\scripts\Invoke-CarrierLabRemeshGuard.ps1' -Cached
```

Include repo skill docs in the scan only when editing skill instructions:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File '<skill-dir>\scripts\Invoke-CarrierLabRemeshGuard.ps1' -IncludeSkills
```

## Contract

Use `docs/paper-resampling-extraction.md` as the remesh authority.

Paper-faithful remesh must:

- Cast ray from planet center through each global TDS vertex.
- Ignore subducting and colliding triangles before source selection.
- Use a remaining valid hit for barycentric crust interpolation.
- Treat zero valid hits as divergent gap fill with continuous q1/q2 from two
  different plate boundaries and qGamma midpoint provenance.
- Rebuild plate-local topology by duplicate/re-index/re-compact.
- Reset subduction marks, active convergence lists, distance-to-front records,
  and subduction matrix at remesh.

Forbidden in a primary remesh path:

- prior global sample owner/fraction fallback
- centroid/random/synthetic policy as a winner
- recovery/repair/backfill/retention/hysteresis/anchoring logic
- discrete endpoint/midpoint q1/q2 as the authoritative source after IIIE.2
- unresolved multi-hit silently resolved instead of counted and gated

## Review Pattern

1. Run the scan.
2. Read each hit in context; the scanner is heuristic and may produce false
   positives in docs or comparison controls.
3. Classify every hit as `primary-path risk`, `diagnostic/control only`, or
   `false positive`.
4. If remesh behavior changed, require an independent oracle/report gate.
5. Do not proceed to implementation or commit if a primary-path risk remains.
