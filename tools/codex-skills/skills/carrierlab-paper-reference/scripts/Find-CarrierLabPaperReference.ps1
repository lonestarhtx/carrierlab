param(
    [Parameter(Mandatory=$true)]
    [string]$Query,
    [string]$RepoRoot = (Get-Location).Path
)

$docs = Join-Path $RepoRoot 'docs'
if (-not (Test-Path $docs)) {
    throw "docs folder not found under $RepoRoot"
}

$patterns = @('*.md','*.txt','*.tex')
$files = foreach ($pattern in $patterns) {
    Get-ChildItem -Path $docs -Recurse -File -Filter $pattern -ErrorAction SilentlyContinue
}

$matches = foreach ($file in $files) {
    Select-String -Path $file.FullName -Pattern $Query -SimpleMatch -Context 1,2 -ErrorAction SilentlyContinue |
        Select-Object @{Name='Path';Expression={$_.Path}}, LineNumber, Line
}

$pdfs = Get-ChildItem -Path $docs -Recurse -File -Filter '*.pdf' -ErrorAction SilentlyContinue |
    Select-Object FullName
$pngs = Get-ChildItem -Path $docs -Recurse -File -Filter '*.png' -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -match $Query -or $_.DirectoryName -match $Query } |
    Select-Object FullName

[pscustomobject]@{
    query = $Query
    text_matches = @($matches)
    pdf_sources = @($pdfs)
    image_candidates = @($pngs)
} | ConvertTo-Json -Depth 5
