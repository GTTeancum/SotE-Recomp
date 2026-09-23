<#
.SYNOPSIS
    Verify loose PC SAN/SMUSH movie files with the local FFmpeg decoder.

.DESCRIPTION
    Decodes every root-level .SAN file in the runnable Sdata directory to
    FFmpeg's null muxer and saves the first decoded frame as a PNG preview.
    This is an external verification tool: it proves the user's local movie
    files are structurally decodable without checking any movie asset into git.
#>
[CmdletBinding()]
param(
    [string]$InputDirectory = 'SotE_Recompiled\Sdata',
    [string]$OutputDirectory = 'build\san_movie_verification'
)

$ErrorActionPreference = 'Stop'
$repoRoot = [System.IO.Path]::GetFullPath(
    (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)))
$inputAbsolute = if ([System.IO.Path]::IsPathRooted($InputDirectory)) {
    [System.IO.Path]::GetFullPath($InputDirectory)
}
else {
    [System.IO.Path]::GetFullPath((Join-Path $repoRoot $InputDirectory))
}
$outputAbsolute = if ([System.IO.Path]::IsPathRooted($OutputDirectory)) {
    [System.IO.Path]::GetFullPath($OutputDirectory)
}
else {
    [System.IO.Path]::GetFullPath((Join-Path $repoRoot $OutputDirectory))
}

if (-not (Get-Command ffmpeg -ErrorAction SilentlyContinue)) {
    throw 'ffmpeg is not available on PATH.'
}
if (-not (Test-Path -LiteralPath $inputAbsolute -PathType Container)) {
    throw "Missing SAN input directory: $inputAbsolute"
}

New-Item -ItemType Directory -Path $outputAbsolute -Force | Out-Null
$movies = @(Get-ChildItem -LiteralPath $inputAbsolute -Filter '*.SAN' -File |
    Sort-Object Name)
if ($movies.Count -eq 0) {
    throw "No root-level .SAN files found in $inputAbsolute"
}

$manifest = New-Object System.Collections.Generic.List[string]
$manifest.Add("input`tname`tbytes`tpreview`tstatus")
$failed = 0
foreach ($movie in $movies) {
    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($movie.Name)
    $preview = Join-Path $outputAbsolute "$baseName.png"
    $status = 'ok'
    & ffmpeg -hide_banner -loglevel error -y `
        -i $movie.FullName -frames:v 1 -update 1 $preview
    if ($LASTEXITCODE -ne 0) {
        $status = 'preview_failed'
        ++$failed
    }
    else {
        & ffmpeg -hide_banner -loglevel error -y `
            -i $movie.FullName -f null NUL
        if ($LASTEXITCODE -ne 0) {
            $status = 'decode_failed'
            ++$failed
        }
    }
    $manifest.Add(
        "$inputAbsolute`t$($movie.Name)`t$($movie.Length)`t$preview`t$status")
}

$manifestPath = Join-Path $outputAbsolute 'manifest.tsv'
$manifest | Set-Content -LiteralPath $manifestPath -Encoding UTF8

Write-Host "Checked $($movies.Count) SAN file(s)."
Write-Host "Manifest: $manifestPath"
if ($failed -ne 0) {
    throw "$failed SAN file(s) failed FFmpeg verification."
}
