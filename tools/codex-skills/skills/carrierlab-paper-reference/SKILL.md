---
name: carrierlab-paper-reference
description: Look up paper and thesis details for CarrierLab. Use when a slice depends on Cortial paper/thesis formulas, constants, figures, tables, process pseudocode, carrier semantics, or when the user asks whether implementation still matches the paper.
---

# CarrierLab Paper Reference

## Local Sources

Prefer local files in the repo:

- `docs/ProceduralTectonicPlanets/ProceduralTectonicPlanets.pdf`
- `docs/ProceduralTectonicPlanets/ProceduralTectonicPlanets.txt` when present
- `docs/Synthèse de terrain à léchelle planétaire/Synthèse de terrain à l'échelle planétaire.pdf`
- PNG page exports in the same folders when visual figures matter
- existing extracted notes in `docs/paper-carrier-extraction.md`,
  `docs/paper-resampling-extraction.md`, `docs/carrier-design.md`, and
  checkpoint reports

Use text extraction for searchable text, but inspect PNG/page images for diagrams, formulas, and visual morphology.

## Quick Search

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File '<skill-dir>\scripts\Find-CarrierLabPaperReference.ps1' -Query 'uplift'
```

## Anchor Reference

Read `references/phase-iii-paper-anchors.md` when you need known section/table/figure anchors.

## Discipline

- Quote or cite the local source path/section in docs or reports.
- If a figure formula is used, inspect the image or PDF page, not text only.
- Separate paper-faithful behavior from lab policy.
- If a thesis detail is underspecified, write that explicitly and gate implementation as a lab policy.
