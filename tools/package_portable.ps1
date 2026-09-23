<#
.SYNOPSIS
    Create a self-contained Shadows of the Empire play folder.

.DESCRIPTION
    Copies the built executable, runtime DLLs, retail image, matching ROM, and
    existing save and optional external music into one portable directory. Packaged saves are
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
# Canonical No-Intro 'Star Wars - Shadows of the Empire (USA) (Rev 2)',
# plus a widely mirrored pack copy that differs only at ROM offset
# 0x3B7B6E, outside every recompiled section, so it runs identically.
$acceptedSha256 = @(
    'E7085E013123537F34E0EDEC8801318016DA4DBAC424172D6DC5F3B67D98642C'
    '2802BF4135842F7C8D254349ED7AC2641F6D7FF45E9D2D01304E1455706DD103'
)

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
if ($acceptedSha256 -notcontains $actualSha256) {
    throw "Unsupported ROM SHA-256 $actualSha256"
}

New-Item -ItemType Directory -Path $outputAbsolute -Force | Out-Null
foreach ($entry in $sources.GetEnumerator()) {
    Copy-Item -LiteralPath $entry.Value `
        -Destination (Join-Path $outputAbsolute $entry.Key) -Force
}

# Optional external soundtrack. Merge source assets/maps into the actual play
# folder, including the corrected map. Never clear existing user-owned music.
$sourceMusic = Join-Path $repoRoot 'Sdata\MUSIC'
$packagedMusic = Join-Path $outputAbsolute 'Sdata\MUSIC'
if ((Test-Path -LiteralPath $sourceMusic -PathType Container) -and
    -not [string]::Equals($sourceMusic, $packagedMusic,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    New-Item -ItemType Directory -Path $packagedMusic -Force | Out-Null
    foreach ($musicFile in (Get-ChildItem -LiteralPath $sourceMusic -File -Recurse)) {
        if ($musicFile.Extension.ToLowerInvariant() -notin @('.ogg', '.tsv')) {
            continue
        }
        $relativeMusic = $musicFile.FullName.Substring($sourceMusic.Length + 1)
        $destinationMusic = Join-Path $packagedMusic $relativeMusic
        New-Item -ItemType Directory -Path (Split-Path -Parent $destinationMusic) `
            -Force | Out-Null
        Copy-Item -LiteralPath $musicFile.FullName -Destination $destinationMusic -Force
    }
}
Copy-Item -LiteralPath (Join-Path $repoRoot 'docs\EXTERNAL_MUSIC.md') `
    -Destination (Join-Path $outputAbsolute 'EXTERNAL_MUSIC.md') -Force

# Merge menu configuration and user-supplied fonts; never delete unrelated UI assets.
$sourceUi = Join-Path $repoRoot 'Sdata\UI'
$packagedUi = Join-Path $outputAbsolute 'Sdata\UI'
if ((Test-Path -LiteralPath $sourceUi -PathType Container) -and
    -not [string]::Equals($sourceUi, $packagedUi,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    foreach ($uiFile in (Get-ChildItem -LiteralPath $sourceUi -File -Recurse)) {
        $relativeUi = $uiFile.FullName.Substring($sourceUi.Length + 1)
        $destinationUi = Join-Path $packagedUi $relativeUi
        New-Item -ItemType Directory -Path (Split-Path -Parent $destinationUi) -Force | Out-Null
        Copy-Item -LiteralPath $uiFile.FullName -Destination $destinationUi -Force
    }
}
Copy-Item -LiteralPath (Join-Path $repoRoot 'docs\MENU_REVAMP.md') `
    -Destination (Join-Path $outputAbsolute 'MENU_REVAMP.md') -Force

# Binding settings live under the runnable Sdata directory. Never deploy a
# source default over controls_bindings.ini; only copy the user documentation.
Copy-Item -LiteralPath (Join-Path $repoRoot 'docs\CONTROL_REBINDING.md') `
    -Destination (Join-Path $outputAbsolute 'CONTROL_REBINDING.md') -Force

$packagedSaveDirectory = Join-Path $outputAbsolute 'saves'
$textureToolsDirectory = Join-Path $outputAbsolute 'texture_tools'
New-Item -ItemType Directory -Path $textureToolsDirectory -Force | Out-Null
foreach ($name in @(
    'extract_rom_textures.py', 'extract_rom_segments.py', 'rt64_tmem_hash.py',
    'sote_texture_pack.py', 'convert_rt64_texture_dumps.py',
    'build_dynamic_texture_pack.py', 'build_user_texture_pack.py',
    'verify_tmem_hash.py', 'texture_coverage_capture.ps1', 'requirements.txt'
)) {
    Copy-Item -LiteralPath (Join-Path $repoRoot "tools\$name") -Destination $textureToolsDirectory -Force
}
Copy-Item -LiteralPath (Join-Path $repoRoot 'docs\TEXTURE_PACKS_USER.md') `
    -Destination (Join-Path $outputAbsolute 'TEXTURE_PACKS.md') -Force
Copy-Item -LiteralPath (Join-Path $repoRoot 'docs\MODERN_AIM.md') `
    -Destination (Join-Path $outputAbsolute 'MODERN_AIM.md') -Force
# No source export or proof/enhancement pack is installed automatically.
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
