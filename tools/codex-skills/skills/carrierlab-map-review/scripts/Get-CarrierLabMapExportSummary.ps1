param(
    [string]$Path = 'Saved/CarrierLab',
    [int]$MaxFiles = 80
)

$root = Resolve-Path -LiteralPath $Path -ErrorAction SilentlyContinue
if (-not $root) {
    throw "Path not found: $Path"
}

$pngs = Get-ChildItem -Path $root.Path -Recurse -File -Filter '*.png' -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTimeUtc -Descending |
    Select-Object -First $MaxFiles

$items = foreach ($png in $pngs) {
    $hash = Get-FileHash -LiteralPath $png.FullName -Algorithm SHA256
    $width = $null
    $height = $null
    try {
        Add-Type -AssemblyName System.Drawing -ErrorAction SilentlyContinue
        $img = [System.Drawing.Image]::FromFile($png.FullName)
        $width = $img.Width
        $height = $img.Height
        $img.Dispose()
    } catch {
        $width = $null
        $height = $null
    }

    [pscustomobject]@{
        name = $png.Name
        path = $png.FullName
        bytes = $png.Length
        last_write_utc = $png.LastWriteTimeUtc.ToString('o')
        width = $width
        height = $height
        sha256 = $hash.Hash.ToLowerInvariant()
    }
}

$groups = $items | Group-Object sha256 | Where-Object { $_.Count -gt 1 } | ForEach-Object {
    [pscustomobject]@{
        sha256 = $_.Name
        count = $_.Count
        files = @($_.Group | Select-Object -ExpandProperty path)
    }
}

[pscustomobject]@{
    root = $root.Path
    png_count_returned = @($items).Count
    duplicate_hash_groups = @($groups)
    pngs = @($items)
} | ConvertTo-Json -Depth 6
