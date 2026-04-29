param(
    [string]$RepoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..\..')).Path,
    [string]$CodexSkillsRoot = 'C:\Users\Michael\.codex\skills'
)

$ErrorActionPreference = 'Stop'

$sourceRoot = Join-Path $RepoRoot 'tools\codex-skills\skills'
if (-not (Test-Path -LiteralPath $sourceRoot)) {
    throw "Missing source skills root: $sourceRoot"
}

New-Item -ItemType Directory -Force -Path $CodexSkillsRoot | Out-Null

$installed = @()
Get-ChildItem -LiteralPath $sourceRoot -Directory | Sort-Object Name | ForEach-Object {
    $target = Join-Path $CodexSkillsRoot $_.Name
    if (Test-Path -LiteralPath $target) {
        Remove-Item -LiteralPath $target -Recurse -Force
    }
    Copy-Item -LiteralPath $_.FullName -Destination $target -Recurse
    $installed += $_.Name
}

[ordered]@{
    status = 'installed'
    source_root = $sourceRoot
    target_root = $CodexSkillsRoot
    skills = $installed
} | ConvertTo-Json -Depth 4
