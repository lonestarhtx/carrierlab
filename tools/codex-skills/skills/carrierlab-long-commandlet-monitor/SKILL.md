---
name: carrierlab-long-commandlet-monitor
description: Run expensive CarrierLab Unreal commandlets with heartbeat monitoring, log-growth checks, idle timeout, hard runtime caps, and concise progress summaries. Use when a commandlet may take minutes, when the user asks to monitor tests, or when avoiding silent 20-minute hangs matters.
---

# CarrierLab Long Commandlet Monitor

## Use When

- A CarrierLab `UnrealEditor-Cmd.exe -run=...` commandlet may take more than a minute.
- The user asks to monitor progress, avoid long hangs, or stop tests without real progress.
- You need evidence separating slow-but-progressing commandlets from idle/hung runs.

## Runner

From the repo root:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File '<skill-dir>\scripts\Run-CarrierLabCommandletMonitored.ps1' -Commandlet 'CarrierLabPhaseIIIE67ApplyPathInvalidRecords'
```

Common options:

```powershell
-AbsLog 'Saved\Logs\CarrierLabMyCommandlet.log'
-ExtraArgs '-SomeFlag -AnotherFlag=Value'
-IdleTimeoutSeconds 180
-HardLimitSeconds 1200
-HeartbeatSeconds 30
```

The script:

- launches `UnrealEditor-Cmd.exe` with `-unattended -nop4 -nosplash -NullRHI -NoSound -stdout -FullStdOutLogOutput`;
- prints heartbeat lines with elapsed time, process status, log size, and recent progress lines;
- kills the commandlet if the log has no growth for `IdleTimeoutSeconds`;
- kills the commandlet if `HardLimitSeconds` is exceeded;
- prints a compact JSON-like summary with exit code, elapsed time, timeout type, log path, and tail lines.

## Rules

- Prefer this runner over ad hoc long commandlet shells.
- If a commandlet was killed for idle timeout, report it as inconclusive/hung, not as a gate failure.
- If progress logs are missing, add progress instrumentation before rerunning long diagnostics.
- Do not let a test run for 20 minutes without heartbeat evidence or log growth.
- Keep `Saved/` metrics/logs uncommitted unless the user explicitly asks.
