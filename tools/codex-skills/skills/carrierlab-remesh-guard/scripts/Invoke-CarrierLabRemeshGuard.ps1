param(
    [switch]$Cached,
    [switch]$IncludeSkills,
    [string]$RepoRoot = (Get-Location).Path
)

$ErrorActionPreference = 'Stop'

function Invoke-Git {
    param([string[]]$GitArgs)
    & git @GitArgs
    if ($LASTEXITCODE -ne 0) {
        throw "git $($GitArgs -join ' ') failed with exit code $LASTEXITCODE"
    }
}

$RepoRoot = (Resolve-Path -LiteralPath $RepoRoot).Path
Push-Location $RepoRoot
try {
    $diffArgs = @('diff', '--name-only')
    if ($Cached) { $diffArgs = @('diff', '--cached', '--name-only') }
    $files = @(Invoke-Git $diffArgs | Where-Object { $_ })
    if (-not $Cached) {
        $files += @(Invoke-Git @('diff', '--cached', '--name-only') | Where-Object { $_ })
        $files += @(Invoke-Git @('ls-files', '--others', '--exclude-standard') | Where-Object { $_ })
    }
    $files = $files | Sort-Object -Unique

    $interesting = $files | Where-Object {
        ($_ -match '^(Source/CarrierLab|docs/)') -or
        ($IncludeSkills -and $_ -match '^tools/codex-skills/skills/carrierlab-[^/]+/SKILL\.md$')
    }

    $patterns = @(
        @{ Id='prior-owner-plate'; Regex='Sample\.PlateId|ResolvedPlateId|Prior.*Plate|Previous.*Plate'; Severity='high'; Hint='Check this is not fallback global sample ownership.' },
        @{ Id='prior-owner-material'; Regex='Sample\.ContinentalFraction|Prior.*Continental|Previous.*Continental'; Severity='high'; Hint='Check this is not fallback global sample material authority.' },
        @{ Id='primary-tiebreak'; Regex='ChooseNearestCandidatePlate|nearest[- ]?centroid|random tie|seeded random|synthetic'; Severity='high'; Hint='Allowed only as comparison/control, not primary remesh.' },
        @{ Id='repair-language'; Regex='Recover|Repair|Heal|Backfill|Resync|Promote|Reclassify|hysteresis|retention|anchoring'; Severity='medium'; Hint='Clean-room forbidden-token review.' },
        @{ Id='discrete-q1q2'; Regex='endpoint|midpoint|BoundaryPoint|q1|q2|qGamma'; Severity='medium'; Hint='After IIIE.2 q1/q2 primary path should be continuous boundary-edge provenance.' },
        @{ Id='missing-filter-cue'; Regex='subducting|colliding|collision|filter|ignore'; Severity='info'; Hint='For remesh code, verify subducting/colliding triangles are filtered before winner selection.' },
        @{ Id='reset-cue'; Regex='Reset|Invalidate|SubductionMatrix|DistanceToFront|ActiveBoundary|SubductingTriangles'; Severity='info'; Hint='For remesh code, verify process state resets at remesh.' }
    )

    $hits = New-Object System.Collections.Generic.List[object]
    foreach ($file in $interesting) {
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

    $summary = [pscustomobject]@{
        mode = $(if ($Cached) { 'cached' } else { 'working-plus-cached' })
        scanned_files = @($interesting).Count
        hit_count = $hits.Count
        high_count = @($hits | Where-Object severity -eq 'high').Count
        medium_count = @($hits | Where-Object severity -eq 'medium').Count
        info_count = @($hits | Where-Object severity -eq 'info').Count
        hits = $hits
    }

    $summary | ConvertTo-Json -Depth 5
}
finally {
    Pop-Location
}
