$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Push-Location -LiteralPath $root
try {
    $taskTmp = Join-Path $root 'build/xbox/tmp'
    New-Item -ItemType Directory -Path $taskTmp -Force | Out-Null
    $oldTemp=$env:TEMP; $oldTmp=$env:TMP; $oldMsystem=$env:MSYSTEM
    $env:TEMP=$taskTmp; $env:TMP=$taskTmp; $env:MSYSTEM='MINGW64'
    python xbox/tools/prepare.py
    if ($LASTEXITCODE) { throw 'Xbox preparation failed.' }
    & 'C:\msys64\usr\bin\bash.exe' --noprofile --norc xbox/tools/build.sh *> build/xbox/build.log
    if ($LASTEXITCODE) { Get-Content build/xbox/build.log -Tail 40; throw 'Xbox build failed.' }
    Write-Output "Built $(Join-Path $root 'build/xbox/disc/default.xbe')"
} finally {
    $env:TEMP=$oldTemp; $env:TMP=$oldTmp; $env:MSYSTEM=$oldMsystem
    Pop-Location
}
