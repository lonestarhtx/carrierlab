---
name: carrierlab-phase-iii
description: Follow CarrierLab Phase III paper-process discipline. Use when drafting, reviewing, or implementing Phase III slices, especially IIIA-IIIG storage, convergence tracking, subduction mutation, collision, oceanic generation, rifting, erosion/elevation evolution, and checkpoint gates.
---

# CarrierLab Phase III

## Read First

Load only the relevant subset before Phase III work:

- `AGENTS.md`
- `docs/phase-iii-planning-packet.md`
- `docs/phase-iii-paper-process-design.md`
- `docs/phase-iii-pre-mortem.md`
- `docs/phase-iii-slice-plan.md`
- latest relevant checkpoint in `docs/checkpoints/`
- for IIIC work, also read `docs/checkpoints/phase-iii-iiic-entry-reconciliation.md`

## Core Contract

- Phase III recreates paper process state on top of the validated carrier foundation.
- Carrier authority remains plate-local geometry/material state.
- Global samples are projection/resampling targets, not persistent authority.
- Vertex positions remain on the unit sphere throughout Phase III.
- Elevation is a scalar field until Phase IV amplification consumes it.
- Phase IV visual terrain/amplification must not leak into Phase III.
- Stage 1.5/Slice 5.5 open evidence remains preserved; later slices may explain pieces but must not claim it is erased.

## Sub-Phase Shape

- IIIA: inert crust/elevation/material fields only.
- IIIB: read-only convergence tracking.
- IIIC: subduction process mutation, including trench, uplift, slab-pull feedback.
- IIID: continental collision/suture behavior.
- IIIE: oceanic generation and divergent gap accounting.
- IIIF: rifting.
- IIIG: per-step elevation evolution.
- IIIH: tectonic-only long-horizon validation.

## Slice Rules

For each slice:

1. State the exact approved slice boundary.
2. Identify which prior checkpoint is the baseline.
3. Add one behavior surface only.
4. Include a bypass/off gate when mutation is opt-in.
5. Use an independent oracle for formulas or expected state.
6. Include zero/single/divergence or other relevant negative controls.
7. Write `docs/checkpoints/phase-iii-slice-<id>-report.md`.
8. Pause for user review before advancing.

## Stop Conditions

Pause and write an investigation checkpoint if:

- projection-derived state becomes carrier authority
- persistent global sample ownership appears
- recovery/repair/backfill/anchoring style logic appears
- slab-pull improves projection metrics while weakening authority conservation
- unit-sphere geometry is displaced before Phase IV
- replay hash determinism breaks
- a report claims more than the current slice proves
