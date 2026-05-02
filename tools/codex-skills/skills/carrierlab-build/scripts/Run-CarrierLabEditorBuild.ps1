param(
    [string]$RepoRoot = 'C:\Users\Michael\Documents\Unreal Projects\CarrierLab',
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.7',
    [switch]$CheckOnly,
    [switch]$AllowRunningEditor
)

$ErrorActionPreference = 'Stop'

$project = Join-Path $RepoRoot 'CarrierLab.uproject'
$buildBat = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$logDir = Join-Path $RepoRoot 'Saved\Logs'
$logPath = Join-Path $logDir 'Build.log'

$running = @(Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue |
    Select-Object Id, ProcessName, CPU)

$preflight = [ordered]@{
    repo_root = $RepoRoot
    project_exists = Test-Path -LiteralPath $project
    build_bat_exists = Test-Path -LiteralPath $buildBat
    running_editor = @($running)
    log_path = $logPath
}

if ($CheckOnly) {
    $preflight | ConvertTo-Json -Depth 5
    exit 0
}

if (-not $preflight.project_exists) {
    throw "Missing project: $project"
}
if (-not $preflight.build_bat_exists) {
    throw "Missing Build.bat: $buildBat"
}
if ($running.Count -gt 0 -and -not $AllowRunningEditor) {
    $preflight.status = 'blocked_running_editor'
    $preflight | ConvertTo-Json -Depth 5
    exit 2
}

New-Item -ItemType Directory -Force -Path $logDir | Out-Null

$start = Get-Date
& $buildBat CarrierLabEditor Win64 Development "-Project=$project" -WaitMutex -NoHotReload *> $logPath
$exitCode = $LASTEXITCODE
$duration = ((Get-Date) - $start).TotalSeconds

$tail = @()
$errors = @()
$sandboxCacheHits = @()
if (Test-Path -LiteralPath $logPath) {
    $logLines = @(Get-Content -LiteralPath $logPath | ForEach-Object { [string]$_ })
    $tail = @($logLines | Select-Object -Last 80)
    $errors = @($logLines | Select-String -Pattern 'error |fatal error|Result: Failed|Unhandled exception' -CaseSensitive:$false | Select-Object -Last 60 | ForEach-Object { $_.Line })

    $sandboxCachePatterns = @(
        'AppData\\Local\\UnrealBuildTool',
        '\bLOCALAPPDATA\b',
        'UnauthorizedAccessException',
        'System\.UnauthorizedAccessException',
        'Access to the path .*AppData',
        'Unable to create directory .*AppData',
        'Requested registry access is not allowed'
    )
    $sandboxCacheRegex = '(' + ($sandboxCachePatterns -join '|') + ')'
    $sandboxCacheHits = @($logLines | Select-String -Pattern $sandboxCacheRegex -CaseSensitive:$false | Select-Object -Last 20 | ForEach-Object { $_.Line })
}

$status = if ($exitCode -eq 0) { 'succeeded' } elseif ($sandboxCacheHits.Count -gt 0) { 'sandbox_cache_wall' } else { 'failed' }
$failureKind = if ($sandboxCacheHits.Count -gt 0) { 'ubt_appdata_sandbox' } elseif ($exitCode -ne 0) { 'build_failed' } else { $null }

[ordered]@{
    status = $status
    exit_code = $exitCode
    failure_kind = $failureKind
    duration_seconds = [math]::Round($duration, 3)
    log_path = $logPath
    notable_errors = $errors
    sandbox_cache_hits = $sandboxCacheHits
    log_tail = $tail
} | ConvertTo-Json -Depth 6

exit $exitCode
