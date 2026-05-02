---
name: carrierlab-commandlet-template
description: Scaffold or review CarrierLab slice commandlets. Use when adding a new Phase/Stage commandlet, JSONL metrics writer, checkpoint report builder, replay fixture, independent oracle, or deterministic gate structure.
---

# CarrierLab Commandlet Template

## Quick Workflow

Generate a checklist/skeleton:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File '<skill-dir>\scripts\New-CarrierLabCommandletTemplate.ps1' -SliceId 'IIIC4' -ClassSuffix 'PhaseIIIC4'
```

## Required Shape

Every new slice commandlet should have:

- `UCarrierLab<Phase>Commandlet` header and source.
- one `Main` that returns `0` only when all gates pass.
- replay A/B for same-seed determinism when behavior is deterministic.
- a bypass/off gate for opt-in mutation slices.
- negative controls relevant to the slice.
- independent oracle logic for formulas and expected state.
- JSONL metrics under `Saved/CarrierLab/.../metrics.jsonl`.
- checkpoint report under `docs/checkpoints/...`.
- explicit scope notes saying what the slice may and may not claim.

## Anti-Patterns

- Do not compare implementation output only against itself.
- Do not make a diagnostic hash from one aggregate reused everywhere.
- Do not hide a failed fixture by deleting it; keep it as a diagnostic if it reveals real geometry.
- Do not add resampling/material mutation paths unless the slice contract says so.
- Do not stage generated `Saved/` artifacts unless the user explicitly asks.

## Good Existing Examples

Inspect nearby commandlets before writing a new one:

- `Source/CarrierLab/Private/CarrierLabPhaseIIIC1Commandlet.cpp`
- `Source/CarrierLab/Private/CarrierLabPhaseIIIC2Commandlet.cpp`
- `Source/CarrierLab/Private/CarrierLabPhaseIIIC3Commandlet.cpp`
- `Source/CarrierLab/Private/CarrierLabPhaseIIIBConsolidationCommandlet.cpp`
