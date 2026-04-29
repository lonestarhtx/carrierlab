---
name: carrierlab-phase-ii
description: Follow CarrierLab Phase II subduction planning and implementation discipline. Use when drafting, reviewing, or implementing Phase II subduction slices, especially Slice 0 harness, contact detection, triangle labels, resampling filters, material accounting, checkpoints, and go/no-go gates.
---

# CarrierLab Phase II

## Read First

Load these before Phase II work:

- `docs/phase-ii-planning-packet.md`
- `docs/phase-ii-subduction-design.md`
- `docs/phase-ii-slice-plan.md`
- `docs/phase-ii-pre-mortem.md`
- latest relevant checkpoint in `docs/checkpoints/`

## Core Contract

- Phase II supplies process state for convergent multi-hit filtering.
- Carrier authority remains plate-local geometry/material.
- Global samples are projection/resampling targets, not persistent process authority.
- Centroid/random policies are comparison controls only.
- Same-material contacts stay ambiguous unless a fixture supplies polarity.
- Third-plate intrusion remains its own class.

## Slice Discipline

- Start with Slice 0 only unless the user explicitly approves a later slice.
- Slice 0 is a no-mutation harness and spec audit.
- Each slice ends with `docs/checkpoints/phase-ii-slice-N-report.md`.
- Advancement requires explicit user go/no-go.

## Required Oracles

- Signed convergence must distinguish forced convergence from forced divergence.
- Polarity-swap fixtures must swap the filtered plate.
- Same-seed replay must match contacts, labels, event logs, and hashes.
- Material or CAF changes require named event records.
- Labels must trace to plate-local triangle ids and contact ids.

## Stop Conditions

Pause and write an investigation checkpoint if:

- contact labels depend on prior global sample ownership
- centroid/random fallback is in the primary path
- sign-aware controls regress to symmetric output
- third-plate intrusion is folded into two-plate subduction
- triangle filtering becomes a broad area-fill mask
- determinism breaks
