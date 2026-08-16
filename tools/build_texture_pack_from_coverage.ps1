[CmdletBinding()]
param(
    [string]$CoverageDirectory = ".\build\diagnostics\texture_coverage",
    [string]$OutputDirectory = ".\build\diagnostics\texture_pack_from_coverage",
    [int]$ChunkSize = 256,
    [int]$Jobs = 8
)

$ErrorActionPreference = "Stop"

$repoRoot = [System.IO.Path]::GetFullPath(
    (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)))
$resolvedCoverage = (Resolve-Path -LiteralPath $CoverageDirectory).Path
$resolvedOutput = [System.IO.Path]::GetFullPath(
    (Join-Path (Get-Location) $OutputDirectory))

if (Test-Path -LiteralPath $resolvedOutput) {
    Remove-Item -LiteralPath $resolvedOutput -Recurse -Force
}
New-Item -ItemType Directory -Path $resolvedOutput -Force | Out-Null

$routes = Get-ChildItem -LiteralPath $resolvedCoverage -Directory |
    Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName "texture_dumps") -PathType Container } |
    Sort-Object Name

if ($routes.Count -eq 0) {
    throw "No route texture_dumps folders found in $resolvedCoverage"
}

foreach ($route in $routes) {
    $dumpDirectory = Join-Path $route.FullName "texture_dumps"
    Write-Host ("Converting {0}" -f $route.Name)
    python (Join-Path $repoRoot "tools\convert_rt64_texture_dumps.py") `
        $dumpDirectory `
        $resolvedOutput `
        --chunk-size $ChunkSize `
        --pack-prefix $route.Name `
        --jobs $Jobs
}

$packCount = (
    Get-ChildItem -LiteralPath $resolvedOutput -Directory |
    Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName "rt64.json") -PathType Leaf }
).Count

Write-Host ("Texture pack written to {0} ({1} child pack(s))" -f $resolvedOutput, $packCount)
