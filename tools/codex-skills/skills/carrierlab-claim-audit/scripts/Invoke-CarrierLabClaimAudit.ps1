param(
    [switch]$ChangedOnly,
    [switch]$IncludeSkills,
    [string]$RepoRoot = (Get-Location).Path
)

$ErrorActionPreference = 'Stop'

function Invoke-Git {
    param([string[]]$GitArgs)
    & git @GitArgs
    if ($LASTEXITCODE -ne 0) { throw "git $($GitArgs -join ' ') failed" }
}

$RepoRoot = (Resolve-Path -LiteralPath $RepoRoot).Path
Push-Location $RepoRoot
try {
    if ($ChangedOnly) {
        $files = @(Invoke-Git @('diff', '--name-only') | Where-Object { $_ -match '\.(md|txt)$' })
        $files += @(Invoke-Git @('diff', '--cached', '--name-only') | Where-Object { $_ -match '\.(md|txt)$' })
        $files += @(Invoke-Git @('ls-files', '--others', '--exclude-standard') | Where-Object { $_ -match '\.(md|txt)$' })
        if (-not $IncludeSkills) {
            $files = $files | Where-Object { $_ -notmatch '^tools/codex-skills/skills/' }
        }
        $files = $files | Sort-Object -Unique
    } else {
        $files = Get-ChildItem -Path (Join-Path $RepoRoot 'docs') -Recurse -File -Include *.md,*.txt |
            ForEach-Object { (Resolve-Path -Relative -LiteralPath $_.FullName) -replace '^[.][\\/]', '' }
        $files += Get-ChildItem -Path (Join-Path $RepoRoot 'tools/codex-skills/skills') -Recurse -File -Filter SKILL.md |
            ForEach-Object { (Resolve-Path -Relative -LiteralPath $_.FullName) -replace '^[.][\\/]', '' }
        $files = $files | Sort-Object -Unique
    }

    $patterns = @(
        @{ Id='stage15-works'; Regex='Stage 1\.5 works|Stage 1\.5.*PASS'; Severity='high'; Hint='Needs foundation-characterization caveat.' },
        @{ Id='paper-faithful-stage15'; Regex='Stage 1\.5.*paper-faithful|paper-faithful.*Stage 1\.5'; Severity='high'; Hint='Standalone Stage 1.5 is not paper-faithful remesh.' },
        @{ Id='prove-language'; Regex='\bproves?\b|\bproved\b|\bproven\b'; Severity='medium'; Hint='Check fixture/diagnostic scope.' },
        @{ Id='validated-foundation'; Regex='validated carrier foundation|proven base|carrier foundation.*validated'; Severity='medium'; Hint='Avoid hiding Stage 1.5 limitation.' },
        @{ Id='signature-claim'; Regex='independent signature|signature gate|expected token'; Severity='medium'; Hint='Confirm computed-vs-expected comparison exists.' },
        @{ Id='visual-proof'; Regex='visual.*proof|PNG.*proof|map.*proof|looks sensical|visual validation'; Severity='medium'; Hint='Visuals are diagnostics; metrics/oracles are proof.' },
        @{ Id='tie-policy'; Regex='centroid|random tie|nearest[- ]?centroid|prior-owner|fallback'; Severity='info'; Hint='Ensure lab policy is not primary remesh.' }
    )

    $hits = New-Object System.Collections.Generic.List[object]
    foreach ($file in $files) {
        $path = Join-Path $RepoRoot $file
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { continue }
        $lines = Get-Content -LiteralPath $path
        for ($i = 0; $i -lt $lines.Count; $i++) {
            foreach ($pattern in $patterns) {
                if ($lines[$i] -match $pattern.Regex) {
                    $hits.Add([pscustomobject]@{
                        file = $file
                        line = $i + 1
                        id = $pattern.Id
                        severity = $pattern.Severity
                        text = $lines[$i].Trim()
                        hint = $pattern.Hint
                    })
                }
            }
        }
    }

    [pscustomobject]@{
        mode = $(if ($ChangedOnly) { 'changed-only' } else { 'docs-and-skills' })
        scanned_files = @($files).Count
        hit_count = $hits.Count
        high_count = @($hits | Where-Object severity -eq 'high').Count
        medium_count = @($hits | Where-Object severity -eq 'medium').Count
        info_count = @($hits | Where-Object severity -eq 'info').Count
        hits = $hits
    } | ConvertTo-Json -Depth 5
}
finally {
    Pop-Location
}
