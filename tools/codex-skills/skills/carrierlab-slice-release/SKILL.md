---
name: carrierlab-slice-release
description: Close a CarrierLab implementation slice with the focused release ritual: scoped diff, guardrail scans, CarrierLabEditor build, targeted commandlet, checkpoint report readback, explicit staging, commit, push, and remote SHA verification.
---

# CarrierLab Slice Release

## Quick Workflow

Generate a slice checklist:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File '<skill-dir>\scripts\New-CarrierLabSliceReleaseChecklist.ps1' -SliceName 'Phase III Slice IIIC.4' -Commandlet 'CarrierLabPhaseIIIC4'
```

The helper only prints commands and evidence slots. It does not build, stage, commit, push, or contact the network.

## Closeout Order

1. Confirm `git status --short` and latest commit.
2. Confirm the user-approved slice boundary.
3. Run `git diff --check`.
4. Run forbidden-token scan on touched carrier/process source:
   `Recover|Repair|Heal|Backfill|Resync|Promote|Reclassify|hysteresis|retention|anchoring`
5. Build with `$carrierlab-build` if C++ changed.
6. Run the targeted commandlet/test required by the slice.
7. For IIIC+ convergence-dependent slices, confirm the report includes an explicit IIIB independent-signature regression row (`4df40569f5e51e1a` unless a consolidation checkpoint updates it).
8. Read the generated checkpoint report and metrics before claiming pass.
9. Stage only intended paths. Never use `git add .` in a dirty worktree.
10. Run `git diff --cached --check`.
11. Commit with a narrow message.
12. Push and verify with `$git-push-verify`.

## Windows Git Staging

- If `git add` fails on `.git/index.lock`, verify there is no active competing Git operation.
- Rerun the same scoped `git add -- <paths>` with escalation if needed.
- Do not delete `.git/index.lock` as a first response.

## Final Report

Report:

- commit SHA and message
- build/checks/commandlet run
- checkpoint report path
- pushed branch
- verified local and remote SHA match
- any checks not run
