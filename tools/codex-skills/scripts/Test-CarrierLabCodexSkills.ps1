param(
    [string]$RepoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..\..')).Path,
    [string]$CodexSkillsRoot = 'C:\Users\Michael\.codex\skills'
)

$ErrorActionPreference = 'Stop'

$sourceRoot = Join-Path $RepoRoot 'tools\codex-skills\skills'
if (-not (Test-Path -LiteralPath $sourceRoot)) {
    throw "Missing source skills root: $sourceRoot"
}

$results = @()
Get-ChildItem -LiteralPath $sourceRoot -Directory | Sort-Object Name | ForEach-Object {
    $skillName = $_.Name
    $sourceFiles = @(Get-ChildItem -LiteralPath $_.FullName -Recurse -File | Sort-Object FullName)
    $targetRoot = Join-Path $CodexSkillsRoot $skillName
    $missing = -not (Test-Path -LiteralPath $targetRoot)
    $drift = $false
    $missingFiles = @()

    if (-not $missing) {
        foreach ($sourceFile in $sourceFiles) {
            $relative = $sourceFile.FullName.Substring($_.FullName.Length + 1)
            $targetFile = Join-Path $targetRoot $relative
            if (-not (Test-Path -LiteralPath $targetFile)) {
                $drift = $true
                $missingFiles += $relative
                continue
            }
            $sourceHash = (Get-FileHash -LiteralPath $sourceFile.FullName -Algorithm SHA256).Hash
            $targetHash = (Get-FileHash -LiteralPath $targetFile -Algorithm SHA256).Hash
            if ($sourceHash -ne $targetHash) {
                $drift = $true
            }
        }
    }

    $results += [ordered]@{
        name = $skillName
        installed = -not $missing
        drift = $drift
        missing_files = $missingFiles
    }
}

[ordered]@{
    status = if (($results | Where-Object { -not $_.installed -or $_.drift }).Count -eq 0) { 'ok' } else { 'drift_or_missing' }
    source_root = $sourceRoot
    target_root = $CodexSkillsRoot
    skills = $results
} | ConvertTo-Json -Depth 6
