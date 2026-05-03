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
2. Identify the map timestamp, scenario name, replay, step, Myr, cadence,
   event count, and source metrics/report if present.
3. List PNG dimensions, byte sizes, and SHA256 hashes.
4. Compare hashes across layers to distinguish genuinely empty identical layers from rendering bugs.
5. Show important images with absolute Markdown image paths when answering in the Codex app.
6. Read paired metrics/checkpoint JSONL before concluding from visuals alone.

## Explain-A-Map Pattern

When the user asks "what am I looking at?":

1. Name the layer and its source fields.
2. State what each visible color means. If the image has no embedded legend,
   say that and infer only from the exporter/report code.
3. State the time context: step, Myr, resample event count, cadence, and replay.
4. Explain whether the layer is a snapshot, accumulated window state, or
   post-remesh state.
5. Compare against one or two companion maps that should spatially line up
   (for example PlateBoundaries, CrustType, SubductionRoles, ElevationHeatmap).
6. Call out artifacts separately from model behavior: projection seams,
   Mollweide poles, empty-state identical hashes, sparse tracking windows,
   or stale pre-resample state.

## Interpretation Rules

- Identical hashes can be valid when two layers have empty state and render background only.
- Empty baseline layers should be paired with a non-empty forced fixture to demonstrate the layer can light up.
- Images are diagnostics, not authority. Numbers first, images second, verdict last.
- Do not infer terrain morphology from Phase III heatmaps; Phase IV amplification is where geometry appears.
- Aurous-style maps are communication references, not authority. Prefer clear
  legends, source fields, and time labels over aesthetic similarity.
