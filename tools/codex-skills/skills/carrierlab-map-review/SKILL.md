---
name: carrierlab-map-review
description: Inspect CarrierLab map exports and diagnostic images. Use when checking latest Phase/Stage PNG exports, explaining contact sheets, detecting empty layers, comparing layer hashes/dimensions, or deciding whether visual maps reflect real state or blank diagnostics.
---

# CarrierLab Map Review

## Quick Workflow

Summarize a map/export folder:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File '<skill-dir>\scripts\Get-CarrierLabMapExportSummary.ps1'
```

For a specific folder:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File '<skill-dir>\scripts\Get-CarrierLabMapExportSummary.ps1' -Path 'Saved/CarrierLab/PhaseIII/Observability/Maps'
```

## Review Pattern

1. Identify the latest run folder and fixture/replay.
2. List PNG dimensions, byte sizes, and SHA256 hashes.
3. Compare hashes across layers to distinguish genuinely empty identical layers from rendering bugs.
4. Show important images with absolute Markdown image paths when answering in the Codex app.
5. Read paired metrics/checkpoint JSONL before concluding from visuals alone.

## Interpretation Rules

- Identical hashes can be valid when two layers have empty state and render background only.
- Empty baseline layers should be paired with a non-empty forced fixture to prove the layer can light up.
- Images are diagnostics, not authority. Numbers first, images second, verdict last.
- Do not infer terrain morphology from Phase III heatmaps; Phase IV amplification is where geometry appears.
