---
name: carrierlab-skill-sync
description: Compare and synchronize CarrierLab repo-mirrored skills under tools/codex-skills/skills with the live Codex skills directory. Use when skills feel stale, after editing repo skill mirrors, before relying on a newly created skill, or when checking whether global runtime skills match committed project skills.
---

# CarrierLab Skill Sync

## Compare

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File '<skill-dir>\scripts\Compare-CarrierLabSkillMirror.ps1'
```

## Install / Update Live Copies

The helper supports copying repo-mirrored skills into the live Codex skill
folder:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File '<skill-dir>\scripts\Compare-CarrierLabSkillMirror.ps1' -Install
```

Installing writes outside the repo to `C:\Users\Michael\.codex\skills`, so it
may require escalation/approval in Codex.

## Rules

- Repo mirror is source of truth for CarrierLab project skills.
- Live global skills are runtime copies.
- Never silently overwrite unrelated non-CarrierLab skills.
- After install, rerun compare and expect all CarrierLab skills to be `same`.
- Commit repo-mirror skill changes before treating them as durable.
