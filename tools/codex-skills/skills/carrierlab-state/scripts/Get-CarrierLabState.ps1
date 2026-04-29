param(
    [string]$RepoRoot = (Get-Location).Path,
    [switch]$Docs
)

$ErrorActionPreference = 'Stop'

function Invoke-Git {
    param([string[]]$GitArgs)
    try {
        (& git -C $RepoRoot @GitArgs) -join "`n"
    } catch {
        $null
    }
}

$head = Invoke-Git -GitArgs @('rev-parse', 'HEAD')
$branch = Invoke-Git -GitArgs @('branch', '--show-current')
$status = Invoke-Git -GitArgs @('status', '--short')
$remote = Invoke-Git -GitArgs @('remote', 'get-url', 'origin')
$last = Invoke-Git -GitArgs @('log', '-1', '--oneline')

$result = [ordered]@{
    repo_root = (Resolve-Path -LiteralPath $RepoRoot).Path
    branch = $branch
    head = $head
    last_commit = $last
    remote_origin = $remote
    dirty = -not [string]::IsNullOrWhiteSpace($status)
    status_short = @()
    key_files = [ordered]@{
        agents = Test-Path -LiteralPath (Join-Path $RepoRoot 'AGENTS.md')
        phase_ii_packet = Test-Path -LiteralPath (Join-Path $RepoRoot 'docs/phase-ii-planning-packet.md')
        phase_ii_design = Test-Path -LiteralPath (Join-Path $RepoRoot 'docs/phase-ii-subduction-design.md')
        phase_ii_slice_plan = Test-Path -LiteralPath (Join-Path $RepoRoot 'docs/phase-ii-slice-plan.md')
        phase_ii_pre_mortem = Test-Path -LiteralPath (Join-Path $RepoRoot 'docs/phase-ii-pre-mortem.md')
    }
}

if (-not [string]::IsNullOrWhiteSpace($status)) {
    $result.status_short = @($status -split "`n")
}

if ($Docs) {
    $docRoot = Join-Path $RepoRoot 'docs'
    $result.docs = @()
    if (Test-Path -LiteralPath $docRoot) {
        $result.docs = @(Get-ChildItem -LiteralPath $docRoot -Recurse -File |
            Sort-Object FullName |
            ForEach-Object { $_.FullName.Substring($RepoRoot.Length + 1) })
    }
}

$result | ConvertTo-Json -Depth 6
