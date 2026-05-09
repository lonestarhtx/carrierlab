param(
    [Parameter(Mandatory = $true)]
    [string]$Commandlet,

    [string]$Project = "C:\Users\Michael\Documents\Unreal Projects\CarrierLab\CarrierLab.uproject",

    [string]$Editor = "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe",

    [string]$AbsLog = "",

    [string]$ExtraArgs = "",

    [int]$HeartbeatSeconds = 30,

    [int]$IdleTimeoutSeconds = 180,

    [int]$HardLimitSeconds = 1200
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $Project)) {
    throw "Project not found: $Project"
}
if (-not (Test-Path -LiteralPath $Editor)) {
    throw "UnrealEditor-Cmd.exe not found: $Editor"
}

$RepoRoot = Split-Path -Parent $Project
if ([string]::IsNullOrWhiteSpace($AbsLog)) {
    $SafeName = ($Commandlet -replace '[^A-Za-z0-9_.-]', '_')
    $AbsLog = Join-Path $RepoRoot "Saved\Logs\$SafeName.monitored.log"
}
elseif (-not [System.IO.Path]::IsPathRooted($AbsLog)) {
    $AbsLog = Join-Path $RepoRoot $AbsLog
}

$LogDir = Split-Path -Parent $AbsLog
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
if (Test-Path -LiteralPath $AbsLog) {
    Remove-Item -LiteralPath $AbsLog -Force
}

function Quote-Arg([string]$Value) {
    if ($Value -match '\s|["]') {
        return '"' + ($Value -replace '"', '\"') + '"'
    }
    return $Value
}

$ArgParts = @(
    (Quote-Arg $Project),
    "-run=$Commandlet",
    "-unattended",
    "-nop4",
    "-nosplash",
    "-NullRHI",
    "-NoSound",
    "-stdout",
    "-FullStdOutLogOutput",
    ("-abslog=" + (Quote-Arg $AbsLog))
)
if (-not [string]::IsNullOrWhiteSpace($ExtraArgs)) {
    $ArgParts += $ExtraArgs
}
$ArgString = ($ArgParts -join " ")

Write-Host "CarrierLab monitored commandlet start"
Write-Host "  commandlet: $Commandlet"
Write-Host "  log: $AbsLog"
Write-Host "  idle timeout: ${IdleTimeoutSeconds}s"
Write-Host "  hard limit: ${HardLimitSeconds}s"

$Start = Get-Date
$Process = Start-Process -FilePath $Editor -ArgumentList $ArgString -WorkingDirectory $RepoRoot -PassThru -WindowStyle Hidden
$LastSize = -1L
$LastGrowth = Get-Date
$TimeoutKind = ""
$ExitCode = $null

try {
    while (-not $Process.HasExited) {
        Start-Sleep -Seconds $HeartbeatSeconds
        $Now = Get-Date
        $Elapsed = [int]($Now - $Start).TotalSeconds
        $Size = 0L
        if (Test-Path -LiteralPath $AbsLog) {
            $Size = (Get-Item -LiteralPath $AbsLog).Length
        }
        if ($Size -ne $LastSize) {
            $LastSize = $Size
            $LastGrowth = $Now
        }

        $Idle = [int]($Now - $LastGrowth).TotalSeconds
        Write-Host ("heartbeat elapsed={0}s idle={1}s log_bytes={2}" -f $Elapsed, $Idle, $Size)

        if (Test-Path -LiteralPath $AbsLog) {
            $Progress = Get-Content -LiteralPath $AbsLog -Tail 12 -ErrorAction SilentlyContinue |
                Where-Object { $_ -match 'progress|gate|passed|failed|error|fatal|invalid|hold|record|selection|completed' } |
                Select-Object -Last 4
            foreach ($Line in $Progress) {
                Write-Host "  $Line"
            }
        }

        if ($Elapsed -ge $HardLimitSeconds) {
            $TimeoutKind = "hard_limit"
            Stop-Process -Id $Process.Id -Force
            break
        }
        if ($Idle -ge $IdleTimeoutSeconds) {
            $TimeoutKind = "idle_timeout"
            Stop-Process -Id $Process.Id -Force
            break
        }
    }
}
finally {
    $Process.Refresh()
    if ($Process.HasExited) {
        $ExitCode = $Process.ExitCode
    }
}

$End = Get-Date
$Total = [int]($End - $Start).TotalSeconds
$Tail = @()
if (Test-Path -LiteralPath $AbsLog) {
    $Tail = Get-Content -LiteralPath $AbsLog -Tail 20 -ErrorAction SilentlyContinue
}

Write-Host "CarrierLab monitored commandlet summary"
Write-Host ("  commandlet: {0}" -f $Commandlet)
Write-Host ("  exit_code: {0}" -f $(if ($null -eq $ExitCode) { "null" } else { $ExitCode }))
Write-Host ("  timeout: {0}" -f $(if ([string]::IsNullOrWhiteSpace($TimeoutKind)) { "none" } else { $TimeoutKind }))
Write-Host ("  elapsed_seconds: {0}" -f $Total)
Write-Host ("  log: {0}" -f $AbsLog)
Write-Host "  tail:"
foreach ($Line in $Tail) {
    Write-Host "    $Line"
}

if (-not [string]::IsNullOrWhiteSpace($TimeoutKind)) {
    exit 124
}
if ($null -eq $ExitCode) {
    exit 125
}
exit $ExitCode
