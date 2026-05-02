---
name: carrierlab-build
description: Build and diagnose the CarrierLab Unreal editor target on Windows. Use when compiling CarrierLabEditor, checking Unreal process locks, recognizing UBT AppData cache sandbox failures, separating build success from editor/module-load issues, or needing the exact UE 5.7 build command for this repo.
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

- If the helper returns `status: sandbox_cache_wall`, treat the first run as an environment/sandbox failure, not a compile result.
- The sandbox cache wall usually mentions `AppData\Local\UnrealBuildTool`, `%LOCALAPPDATA%`, `UnauthorizedAccessException`, `Access to the path`, or `Requested registry access is not allowed`.
- On `sandbox_cache_wall`, rerun the same helper/build command outside the sandbox with escalation so UBT can use its AppData cache and reveal the real C++ result. Do not change the project or cache path to work around it.
- If UnrealEditor or UnrealEditor-Cmd is running, ask before stopping it.
- A successful build does not prove the module loaded in the editor; inspect editor logs separately if runtime load fails.
