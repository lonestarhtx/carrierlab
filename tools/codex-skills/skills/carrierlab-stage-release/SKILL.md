---
name: carrierlab-stage-release
description: Finish a CarrierLab stage, slice, docs packet, or implementation follow-up with the repeatable checklist: status, scoped diff, required docs/checkpoint, build or validation, clean-room guardrails, intentional commit, push, and git ls-remote verification.
---

# CarrierLab Stage Release

## Quick Workflow

Generate the checklist:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File '<skill-dir>\scripts\New-CarrierLabReleaseChecklist.ps1' -SliceName 'Phase II Slice 0'
```

The helper only prints a checklist. It does not build, stage, commit, push, or contact the network.

## Release Order

1. Run `$carrierlab-state` or inspect `git status --short`, latest commit, and relevant docs.
2. Confirm the user-approved stage/slice boundary.
3. Review the diff for scope creep and forbidden authority patterns.
4. If C++/Unreal files changed, run `$carrierlab-build`.
5. If docs only changed, run `git diff --check` and any relevant text scans.
6. Ensure checkpoint/report docs exist when closing a stage or slice.
7. Stage only intended files.
8. Commit with a narrow message.
9. Push, then verify remote SHA with `git ls-remote origin <branch>`.

## Guardrail Scan

For carrier/subduction source files, scan for forbidden drift terms:

```powershell
rg -n "Recover|Repair|Heal|Backfill|Resync|Promote|Reclassify|hysteresis|retention|anchoring" Source docs
```

Intentional negative mentions in docs are fine; hidden implementation paths are not.

## Final Answer Evidence

Report:

- commit SHA and message
- build/checks run
- pushed branch
- local HEAD and remote SHA match
- any checks not run
