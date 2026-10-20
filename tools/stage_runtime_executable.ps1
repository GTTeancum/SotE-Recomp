[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$source = Join-Path $root 'build/runtime/Release/sote_recomp.exe'
$folder = Join-Path $root 'SotE_Recompiled'
$expected = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
# Update the actual player-facing launcher first. A locked file must fail the
# staging operation rather than leaving a successful-looking internal copy.
$names = @('Shadows of the Empire.exe', 'sote_recomp.exe')
foreach ($name in $names) {
    $destination = Join-Path $folder $name
    Copy-Item -LiteralPath $source -Destination $destination
    if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -ne $expected) {
        throw "Staged executable does not match build: $name"
    }
}
Write-Host "Both staged launchers verified: $expected"
