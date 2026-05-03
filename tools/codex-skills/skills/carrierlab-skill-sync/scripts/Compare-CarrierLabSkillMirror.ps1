param(
    [string]$RepoRoot = (Get-Location).Path,
    [string]$LiveSkillsRoot = "$env:USERPROFILE\.codex\skills",
    [switch]$Install
)

$ErrorActionPreference = 'Stop'

function Get-DirectoryHash {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Container)) { return $null }
    $hashInput = New-Object System.Text.StringBuilder
    Get-ChildItem -LiteralPath $Path -Recurse -File | Sort-Object FullName | ForEach-Object {
        $rel = $_.FullName.Substring($Path.Length) -replace '^[\\/]', ''
        $fileHash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
        [void]$hashInput.AppendLine("$rel=$fileHash")
    }
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($hashInput.ToString())
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        return ([System.BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $sha.Dispose()
    }
}

$RepoRoot = (Resolve-Path -LiteralPath $RepoRoot).Path
$MirrorRoot = Join-Path $RepoRoot 'tools/codex-skills/skills'
if (-not (Test-Path -LiteralPath $MirrorRoot -PathType Container)) {
    throw "Repo skill mirror not found: $MirrorRoot"
}

if ($Install -and -not (Test-Path -LiteralPath $LiveSkillsRoot -PathType Container)) {
    New-Item -ItemType Directory -Path $LiveSkillsRoot | Out-Null
}

$rows = New-Object System.Collections.Generic.List[object]
Get-ChildItem -LiteralPath $MirrorRoot -Directory | Where-Object { $_.Name -like 'carrierlab-*' } | Sort-Object Name | ForEach-Object {
    $mirror = $_.FullName
    $live = Join-Path $LiveSkillsRoot $_.Name
    $mirrorHash = Get-DirectoryHash $mirror
    $liveHash = Get-DirectoryHash $live
    $status = if (-not $liveHash) { 'missing-live' } elseif ($mirrorHash -eq $liveHash) { 'same' } else { 'different' }

    if ($Install -and $status -ne 'same') {
        if (Test-Path -LiteralPath $live -PathType Container) {
            Remove-Item -LiteralPath $live -Recurse -Force
        }
        Copy-Item -LiteralPath $mirror -Destination $live -Recurse
        $liveHash = Get-DirectoryHash $live
        $status = if ($mirrorHash -eq $liveHash) { 'installed' } else { 'install-mismatch' }
    }

    $rows.Add([pscustomobject]@{
        skill = $_.Name
        status = $status
        mirror_hash = $mirrorHash
        live_hash = $liveHash
    })
}

[pscustomobject]@{
    repo_root = $RepoRoot
    mirror_root = $MirrorRoot
    live_root = $LiveSkillsRoot
    install = [bool]$Install
    total = $rows.Count
    same = @($rows | Where-Object status -eq 'same').Count
    missing_live = @($rows | Where-Object status -eq 'missing-live').Count
    different = @($rows | Where-Object status -eq 'different').Count
    installed = @($rows | Where-Object status -eq 'installed').Count
    rows = $rows
} | ConvertTo-Json -Depth 4
