[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$source = Join-Path $root 'build/runtime/Release/sote_recomp.exe'
$folder = Join-Path $root 'SotE_Recompiled'
$expected = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
Copy-Item -LiteralPath (Join-Path $root 'build/runtime/Release/sandec.dll') -Destination $folder
# Deploy and verify the player-facing launcher.
$destination = Join-Path $folder 'Shadows of the Empire.exe'
Copy-Item -LiteralPath $source -Destination $destination
if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -ne $expected) {
    throw 'Staged executable does not match build: Shadows of the Empire.exe'
}
$decoderSource = Join-Path $folder 'sandec-source'
New-Item -ItemType Directory -Path $decoderSource -Force | Out-Null
foreach ($name in @('sandec.c','sandec.h','LICENSE','README.md')) {
    Copy-Item -LiteralPath (Join-Path $root "third_party/sandec/$name") -Destination $decoderSource -Force
}
Write-Host "Staged launcher verified: $expected"
