<#
.SYNOPSIS
    Create a self-contained Shadows of the Empire play folder.

.DESCRIPTION
    Copies the built executable, runtime DLLs, retail image, matching ROM, and
    existing save into one portable directory. Existing packaged saves are
    preserved.
#>
[CmdletBinding()]
param(
    [string]$RomPath =
        'Star Wars - Shadows of the Empire (U) (V1.2) [!].z64',
    [string]$OutputDirectory = 'SotE_Recompiled'
)

$ErrorActionPreference = 'Stop'
$repoRoot = [System.IO.Path]::GetFullPath(
    (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)))
$romAbsolute = if ([System.IO.Path]::IsPathRooted($RomPath)) {
    [System.IO.Path]::GetFullPath($RomPath)
}
else {
    [System.IO.Path]::GetFullPath((Join-Path $repoRoot $RomPath))
}
$outputAbsolute = if (
    [System.IO.Path]::IsPathRooted($OutputDirectory)) {
    [System.IO.Path]::GetFullPath($OutputDirectory)
}
else {
    [System.IO.Path]::GetFullPath(
        (Join-Path $repoRoot $OutputDirectory))
}
$releaseDirectory = Join-Path $repoRoot 'build\runtime\Release'
$expectedSha256 =
    '2802BF4135842F7C8D254349ED7AC2641F6D7FF45E9D2D01304E1455706DD103'

$sources = @{
    'Shadows of the Empire.exe' =
        (Join-Path $releaseDirectory 'sote_recomp.exe')
    'SDL2.dll' = (Join-Path $releaseDirectory 'SDL2.dll')
    'dxcompiler.dll' = (Join-Path $releaseDirectory 'dxcompiler.dll')
    'dxil.dll' = (Join-Path $releaseDirectory 'dxil.dll')
    'main.bin' = (Join-Path $repoRoot 'generated\main.bin')
    'sote.us.v1.2.z64' = $romAbsolute
}

foreach ($source in $sources.Values) {
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Missing package input: $source"
    }
}

$actualSha256 = (
    Get-FileHash -LiteralPath $romAbsolute -Algorithm SHA256).Hash
if ($actualSha256 -ne $expectedSha256) {
    throw "Unsupported ROM SHA-256 $actualSha256"
}

New-Item -ItemType Directory -Path $outputAbsolute -Force | Out-Null
foreach ($entry in $sources.GetEnumerator()) {
    Copy-Item -LiteralPath $entry.Value `
        -Destination (Join-Path $outputAbsolute $entry.Key) -Force
}

$packagedSaveDirectory = Join-Path $outputAbsolute 'saves'
New-Item -ItemType Directory -Path $packagedSaveDirectory -Force | Out-Null
$packagedSave = Join-Path $packagedSaveDirectory 'sote.us.v1.2.bin'
$developmentSave =
    Join-Path $repoRoot 'out\config\saves\sote.us.v1.2.bin'
if (-not (Test-Path -LiteralPath $packagedSave -PathType Leaf) -and
    (Test-Path -LiteralPath $developmentSave -PathType Leaf)) {
    Copy-Item -LiteralPath $developmentSave `
        -Destination $packagedSave
}

Write-Host ''
Write-Host "Playable folder: $outputAbsolute"
Write-Host 'Double-click "Shadows of the Empire.exe" to play.'
