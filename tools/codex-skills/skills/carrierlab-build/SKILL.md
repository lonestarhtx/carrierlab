---
name: carrierlab-build
description: Build and diagnose the CarrierLab Unreal editor target on Windows. Use when compiling CarrierLabEditor, checking Unreal process locks, separating build success from editor/module-load issues, or needing the exact UE 5.7 build command for this repo.
---

# CarrierLab Build

## Quick Workflow

Use the helper from the repo root:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File '<skill-dir>\scripts\Run-CarrierLabEditorBuild.ps1'
```

Preflight only:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File '<skill-dir>\scripts\Run-CarrierLabEditorBuild.ps1' -CheckOnly
```

## Defaults

- Target: `CarrierLabEditor Win64 Development`
- Project: `C:\Users\Michael\Documents\Unreal Projects\CarrierLab\CarrierLab.uproject`
- Engine: `C:\Program Files\Epic Games\UE_5.7`
- Log: `Saved/Logs/Build.log`

## Notes

- If sandboxed UnrealBuildTool cannot write under `%LOCALAPPDATA%`, rerun the same build with escalation. Do not change the project to avoid the cache path.
- If UnrealEditor or UnrealEditor-Cmd is running, ask before stopping it.
- A successful build does not prove the module loaded in the editor; inspect editor logs separately if runtime load fails.
