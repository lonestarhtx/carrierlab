# Current Code Archive Index

Date: 2026-06-08

Purpose: freeze the current CarrierLab implementation as evidence and lessons before a clean carrier-core restart. This is a non-destructive archive: no source files are moved or deleted by this note.

## Current State

Current branch at inspection time:

- `master`
- `HEAD`: `991ad69 Resolve IIIE6 mixed-material multi-hit holds`

Working tree had existing edits before the rebaseline docs:

- Modified visualization/observability code:
  - `Source/CarrierLab/Private/CarrierLabPhaseIIIEVisualValidationCommandlet.cpp`
  - `Source/CarrierLab/Private/CarrierLabPhaseIIIObservabilityCommandlet.cpp`
  - `Source/CarrierLab/Private/CarrierLabVisualizationActor.cpp`
  - `Source/CarrierLab/Public/CarrierLabVisualizationActor.h`
  - `Source/CarrierLabEditor/Private/SCarrierLabControlPanel.cpp`
- Modified Phase III docs:
  - `docs/phase-iii-paper-process-design.md`
  - `docs/phase-iii-planning-packet.md`
  - `docs/phase-iii-pre-mortem.md`
  - `docs/phase-iii-slice-plan.md`
- Untracked diagnosis/substrate commandlets:
  - `Source/CarrierLab/Private/CarrierLabPhaseIIIE612CoherenceDiagnosisCommandlet.cpp`
  - `Source/CarrierLab/Private/CarrierLabPhaseIIIFCrustFieldSubstrateCommandlet.cpp`
  - `Source/CarrierLab/Public/CarrierLabPhaseIIIE612CoherenceDiagnosisCommandlet.h`
  - `Source/CarrierLab/Public/CarrierLabPhaseIIIFCrustFieldSubstrateCommandlet.h`
- Untracked checkpoints:
  - `docs/checkpoints/phase-iii-current-agent-handoff.md`
  - `docs/checkpoints/phase-iii-slice-iiie6-12-coherence-diagnosis.md`
  - `docs/checkpoints/phase-iii-slice-iiif-crust-field-substrate.md`
- Untracked Rock 3 research notes:
  - `docs/rock3-clean-room-study.md`
  - `docs/rock3-plate-carrier-clean-room-dissection.md`
  - `docs/rock3-metadata-and-ptp-carrier-synthesis.md`
- Untracked local skill mirror:
  - `tools/codex-skills/skills/windows-ssh-codex-remote-setup/`

## What The Current Code Demonstrated

The current code path was not wasted.

It demonstrated:

- Stage/checkpoint commandlets are the right verification style for this project.
- Same-seed replay, hashes, metrics rows, and checkpoint reports are essential.
- Projection, remesh, and visual validation need separate oracles.
- Raw candidate counts must be preserved before resolution.
- Zero-hit and multi-hit cases cannot be hidden behind a final selected owner.
- Maps and contact sheets are useful, but only when tied to numeric gates.
- Per-plate mass/area deltas are required to keep material accounting honest.
- Boundary/process state is not optional once resampling is involved.

## What The Current Code Exposed

The current code path also exposed architectural drift:

- Stage 1.5 was asked to resolve multi-hit cases before the proper convergence/subduction/collision state existed.
- Several resolver policies became diagnostic scaffolding: centroid, synthetic labels, random tie-breaks, nearest-hit, distance fallback, mixed-material holds.
- Later Phase III slices accumulated around symptom containment rather than a small carrier kernel.
- Observability grew in many places, but the authority model remained too mesh/sample-resolution centered.
- Gap/overlap handling repeatedly risked sounding like geometry repair instead of named physical process handling.
- Documentation correctly identified many thesis details, but the implementation surface became too broad to trust as a clean proof path.

## Knowledge To Preserve

Preserve these ideas in the restart:

- Plate-local authority is required by the thesis-literal path.
- Global samples/maps are reads, not authority.
- Projection must record raw hit count, hit classes, boundary degeneracy, and selected result separately.
- Resampling must be impossible to call paper-faithful without upstream subduction/collision filtering.
- Divergent gap fill must use named q1/q2/qGamma provenance, not ownership fallback.
- Material continuity must be independently checked before and after resampling.
- Boundary process state must drive what survives in overlaps.
- Every checkpoint needs numbers first, maps second, verdict last.

## Knowledge Not To Carry Forward Blindly

Do not carry these as defaults:

- Treating standalone Stage 1.5 as thesis-remesh evidence.
- Centroid/random/synthetic multi-hit policies as anything beyond diagnostics.
- Prior-owner fallback or any hidden repair authority.
- Visual map plausibility as a pass condition.
- Phase III commandlet sprawl as the starting point for the new kernel.
- Live actor/control-panel behavior as proof of core correctness.

## Archive Interpretation

The current implementation should be treated as:

- an evidence archive
- a test-idea library
- a cautionary record of failure modes
- a source of useful metrics/reporting patterns

It should not be treated as:

- the foundation for the next carrier kernel
- a nearly-finished paper recreation
- a source of APIs that must be preserved
- a reason to keep patching forward

## Rebaseline Rule

The clean restart should build a new minimal carrier core and only re-import old code intentionally.

Every old component must answer:

1. Which PTP/Cortial contract requirement does this satisfy?
2. Is it source-grounded, diagnostic-only, or lab policy?
3. Can the requirement be met more simply in the new kernel?
4. Does it preserve material authority without hidden global ownership?

If not, leave it archived.
