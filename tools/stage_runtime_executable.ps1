[CmdletBinding()]
param([string]$OutputDirectory = 'SotE_Recompiled')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$launcher = 'Shadows of the Empire.exe'
$source = Join-Path $root ('build/runtime/Release/' + $launcher)
$folder = if ([IO.Path]::IsPathRooted($OutputDirectory)) {
    $OutputDirectory
} else {
    Join-Path $root $OutputDirectory
}
$destination = [IO.Path]::GetFullPath((Join-Path $folder $launcher))
if ($destination -eq [IO.Path]::GetFullPath($source)) {
    throw 'The play folder must be separate from the compiler output.'
}
New-Item -ItemType Directory -Path $folder -Force | Out-Null
$expected = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
if (!(Test-Path -LiteralPath $destination -PathType Leaf) -or
    (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -ne $expected) {
    Copy-Item -LiteralPath $source -Destination $destination -Force
}
if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -ne $expected) {
    throw 'Staged executable does not match build: Shadows of the Empire.exe'
}
$decoder = Join-Path $root 'build/runtime/Release/sandec.dll'
$stagedDecoder = Join-Path $folder 'sandec.dll'
if (!(Test-Path -LiteralPath $stagedDecoder -PathType Leaf) -or
    (Get-FileHash -LiteralPath $stagedDecoder).Hash -ne (Get-FileHash -LiteralPath $decoder).Hash) {
    Copy-Item -LiteralPath $decoder -Destination $stagedDecoder -Force
}
$decoderSource = Join-Path $folder 'sandec-source'
New-Item -ItemType Directory -Path $decoderSource -Force | Out-Null
foreach ($name in @('sandec.c','sandec.h','LICENSE','README.md')) {
    Copy-Item -LiteralPath (Join-Path $root "third_party/sandec/$name") -Destination $decoderSource -Force
}
Write-Host "Staged launcher verified: $expected"
