---
name: carrierlab-stage-release
description: Finish a CarrierLab stage, slice, docs packet, or implementation follow-up with the repeatable checklist: status, scoped diff, required docs/checkpoint, build or validation, clean-room guardrails, scoped git staging, Windows Unreal commandlet/build escalation, .git/index.lock staging escalation, intentional commit, push, and git ls-remote verification.
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
5. Run targeted Unreal commandlets/tests required by the slice.
6. If docs only changed, run `git diff --check` and any relevant text scans.
7. Ensure checkpoint/report docs exist when closing a stage or slice.
8. Stage only intended files.
9. Commit with a narrow message.
10. Push, then verify remote SHA with `git ls-remote origin <branch>`.

## Windows Unreal Commandlets

- In Codex Desktop on Windows, real `UnrealEditor-Cmd.exe -run=...` commandlets should request escalation on the first attempt.
- A sandboxed commandlet launch can return immediately without producing the expected log, report, or output directory. Treat that as an environment launch failure, not commandlet evidence.
- Use explicit `-abslog=<repo>\Saved\Logs\<Commandlet>.log`, then verify the log, checkpoint/report, and expected output folder after the escalated run.
- Do not edit code based only on a no-log/no-report sandboxed commandlet launch.

## Windows Git Staging

- Always stage explicit intended paths, for example `git add -- Source\... docs\...`; do not use `git add .` in a dirty worktree.
- In Codex Desktop on Windows, `git add` may fail with a permission error creating `.git/index.lock` even when the file list is correct.
- If staging fails on `.git/index.lock`, first ensure it is not an active competing Git operation. Then rerun the same scoped `git add -- <paths>` with escalation so Git can write its index lock.
- Do not delete `.git/index.lock` as a first response. Only consider lock cleanup if a stale lock is confirmed and the user approves.

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
