param(
    [string]$Path,
    [string]$RepoRoot = (Get-Location).Path
)

$ErrorActionPreference = 'Stop'

$RepoRoot = (Resolve-Path -LiteralPath $RepoRoot).Path
if ($Path) {
    $files = @($Path)
} else {
    $files = Get-ChildItem -Path (Join-Path $RepoRoot 'Source/CarrierLab/Private') -Recurse -File -Filter '*Commandlet.cpp' |
        ForEach-Object { Resolve-Path -Relative -LiteralPath $_.FullName }
}

$patterns = @(
    @{ Id='expected-nonempty'; Regex='Expected[A-Za-z0-9_]*.*(Strlen|Len)\s*\(|(Strlen|Len)\s*\([^)]*Expected'; Severity='high'; Hint='Expected token non-empty is not a comparison.' },
    @{ Id='closure-only'; Regex='bMetricsHashMatchesComputed'; Severity='medium'; Hint='Closure self-consistency is not expected-token regression by itself.' },
    @{ Id='buildreport-gate'; Regex='const bool b[A-Za-z0-9_]*Gate'; Severity='info'; Hint='Confirm this gate appears in final pass/fail.' },
    @{ Id='final-pass'; Regex='bPass|return .*Passes|return .*b[A-Za-z0-9_]*'; Severity='info'; Hint='Read final commandlet return path.' },
    @{ Id='signature'; Regex='IndependentSignature|ExpectedIIIB|SignatureHash|RollupSignature|ConvergenceTrackingHash'; Severity='medium'; Hint='Confirm recomputed signature is compared to expected token.' },
    @{ Id='oracle'; Regex='Oracle|Residual|Expected'; Severity='info'; Hint='Confirm oracle recomputes from raw fixture/config data.' },
    @{ Id='replay'; Regex='Replay|SameSeed|Run.*A|Run.*B'; Severity='info'; Hint='Confirm A/B are independent reruns.' }
)

$hits = New-Object System.Collections.Generic.List[object]
foreach ($file in $files) {
    $full = if ([System.IO.Path]::IsPathRooted($file)) { $file } else { Join-Path $RepoRoot $file }
    if (-not (Test-Path -LiteralPath $full -PathType Leaf)) { continue }
    $lines = Get-Content -LiteralPath $full
    for ($i = 0; $i -lt $lines.Count; $i++) {
        foreach ($pattern in $patterns) {
            if ($lines[$i] -match $pattern.Regex) {
                $hits.Add([pscustomobject]@{
                    file = ((Resolve-Path -Relative -LiteralPath $full) -replace '^[.][\\/]', '')
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
    scanned_files = @($files).Count
    hit_count = $hits.Count
    high_count = @($hits | Where-Object severity -eq 'high').Count
    medium_count = @($hits | Where-Object severity -eq 'medium').Count
    info_count = @($hits | Where-Object severity -eq 'info').Count
    hits = $hits
} | ConvertTo-Json -Depth 5
