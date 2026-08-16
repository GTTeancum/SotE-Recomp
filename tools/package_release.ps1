[CmdletBinding()]
param(
    [string]$Version = "0.9beta",
    [string]$OutputDirectory = ".\build\release"
)

$ErrorActionPreference = "Stop"

$repoRoot = [System.IO.Path]::GetFullPath(
    (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)))
$resolvedOutput = [System.IO.Path]::GetFullPath(
    (Join-Path (Get-Location) $OutputDirectory))
$staging = Join-Path $resolvedOutput ("SotE-Recomp-{0}" -f $Version)
$zipPath = Join-Path $resolvedOutput ("SotE-Recomp-{0}.zip" -f $Version)

function Assert-UnderPath {
    param(
        [string]$Child,
        [string]$Parent
    )
    $childFull = [System.IO.Path]::GetFullPath($Child)
    $parentFull = [System.IO.Path]::GetFullPath($Parent).TrimEnd('\') + '\'
    if (-not $childFull.StartsWith($parentFull, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to operate outside expected directory: $childFull"
    }
}

New-Item -ItemType Directory -Path $resolvedOutput -Force | Out-Null
Assert-UnderPath -Child $staging -Parent $resolvedOutput
Assert-UnderPath -Child $zipPath -Parent $resolvedOutput

if (Test-Path -LiteralPath $staging) {
    Remove-Item -LiteralPath $staging -Recurse -Force
}
New-Item -ItemType Directory -Path $staging -Force | Out-Null

$buildDir = Join-Path $repoRoot "build\runtime\Release"
$requiredFiles = @(
    @{ Source = Join-Path $buildDir "sote_recomp.exe"; Target = "Shadows of the Empire.exe" },
    @{ Source = Join-Path $buildDir "SDL2.dll"; Target = "SDL2.dll" },
    @{ Source = Join-Path $buildDir "dxcompiler.dll"; Target = "dxcompiler.dll" },
    @{ Source = Join-Path $buildDir "dxil.dll"; Target = "dxil.dll" },
    @{ Source = Join-Path $buildDir "rt64.json"; Target = "rt64.json" },
    @{ Source = Join-Path $buildDir "sote_options.json"; Target = "sote_options.json" },
    @{ Source = Join-Path $repoRoot "config\CONTROLS_MODERN.INI"; Target = "CONTROLS_MODERN.INI" }
)

foreach ($file in $requiredFiles) {
    if (-not (Test-Path -LiteralPath $file.Source -PathType Leaf)) {
        throw "Missing release input: $($file.Source)"
    }
    Copy-Item -LiteralPath $file.Source -Destination (Join-Path $staging $file.Target) -Force
}

$releaseReadme = @"
Shadows of the Empire: Recompiled $Version

This package contains no ROM, extracted executable image, saves, Sdata,
texture dumps, texture packs, or other extracted game data.

To play, place a legally obtained USA v1.2 big-endian ROM next to
Shadows of the Empire.exe using either filename:

  sote.us.v1.2.z64
  Star Wars - Shadows of the Empire (U) (V1.2) [!].z64

Expected ROM SHA-256:
  2802bf4135842f7c8d254349ed7ac2641f6d7ff45e9d2d01304e1455706dd103

Modern controller tuning lives in CONTROLS_MODERN.INI. The file is heavily
commented with valid ranges and notes for each setting. Beta testers are
encouraged to tune it for their controllers and share the best setups.
"@
[System.IO.File]::WriteAllText(
    (Join-Path $staging "README.txt"),
    $releaseReadme,
    [System.Text.UTF8Encoding]::new($false))

$forbiddenNames = @(
    "main.bin",
    "sote.us.v1.2.z64",
    "Star Wars - Shadows of the Empire (U) (V1.2) [!].z64",
    "Sdata",
    "saves",
    "textures",
    "logs",
    "rt64.log",
    "sote-active.log"
)
$stagedItems = Get-ChildItem -LiteralPath $staging -Recurse -Force
foreach ($item in $stagedItems) {
    if ($forbiddenNames -contains $item.Name) {
        throw "Forbidden release content staged: $($item.FullName)"
    }
    if ($item.Extension -in @(".z64", ".n64", ".v64", ".dds", ".wav", ".ogg")) {
        throw "Forbidden release extension staged: $($item.FullName)"
    }
}

if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}
Compress-Archive -Path (Join-Path $staging "*") -DestinationPath $zipPath -CompressionLevel Optimal

Write-Host "Release package: $zipPath"
