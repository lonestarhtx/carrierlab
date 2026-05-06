---
name: carrierlab-state
description: Load current CarrierLab project truth quickly. Use when starting CarrierLab work, checking the active stage or Phase II contract, orienting around docs and git state, or needing a compact repo snapshot before code or planning changes.
---

# CarrierLab State

## Quick Workflow

From the repo root, run the helper for a compact JSON snapshot:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File '<skill-dir>\scripts\Get-CarrierLabState.ps1'
```

Use `-Docs` to include discovered docs and checkpoint paths:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File '<skill-dir>\scripts\Get-CarrierLabState.ps1' -Docs
```

## Always Read First

For CarrierLab implementation or planning work, read only the relevant subset:

- `AGENTS.md` for clean-room and push-verification rules.
- `docs/phase-ii-planning-packet.md` for Phase II entry conditions.
- `docs/phase-ii-subduction-design.md` for the current Phase II authority model.
- `docs/phase-ii-slice-plan.md` for the approved slice boundary.
- Latest checkpoint in `docs/checkpoints/` when changing stage behavior.

## Current Discipline

- Carrier authority is plate-local geometry and material state.
- Global samples are projection/resampling targets, not persistent authority.
- Stage or slice advancement requires a written checkpoint and explicit user go/no-go.
- Numbers first, images second, verdict last.
- Do not port Aurous V6/V9/Prototype A/B/C/D/E logic.

## Codex Desktop Windows Notes

- For release/closeout work, use `$carrierlab-stage-release` before staging, committing, pushing, or running final validation.
- Stage explicit intended paths only. In Codex Desktop on Windows, `git add` can fail on `.git/index.lock` under the normal sandbox; if so, verify no competing Git operation, then rerun the same scoped `git add -- <paths>` with escalation.
- Real `CarrierLabEditor` builds should use `$carrierlab-build` and request escalation on the first compile attempt because UnrealBuildTool writes AppData caches outside the workspace sandbox.
- Real `UnrealEditor-Cmd.exe -run=...` commandlets/tests should request escalation on the first attempt. A sandboxed launch can return immediately without a log/report/output folder; treat that as an environment launch failure, not commandlet evidence.
