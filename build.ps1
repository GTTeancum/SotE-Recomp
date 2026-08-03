<#
.SYNOPSIS
    Regenerate and build Shadows of the Empire: Recompiled on Windows.

.DESCRIPTION
    Validates the USA v1.2 ROM, applies the project dependency patches,
    decompresses the retail executable, runs N64Recomp and RSPRecomp, and
    builds the Release runtime with Visual Studio 2022.

.PARAMETER RomPath
    Path to a legally obtained USA v1.2 big-endian ROM.

.PARAMETER Clean
    Remove the two CMake build trees before rebuilding.

.PARAMETER Regenerate
    Re-run ROM extraction, N64Recomp, and RSPRecomp even when generated
    sources already exist.
#>
[CmdletBinding()]
param(
    [string]$RomPath =
        'Star Wars - Shadows of the Empire (U) (V1.2) [!].z64',
    [switch]$Clean,
    [switch]$Regenerate
)

$ErrorActionPreference = 'Stop'
$repoRoot = [System.IO.Path]::GetFullPath(
    (Split-Path -Parent $MyInvocation.MyCommand.Path))
$romAbsolute = [System.IO.Path]::GetFullPath(
    (Join-Path $repoRoot $RomPath))
$expectedSha256 =
    '2802BF4135842F7C8D254349ED7AC2641F6D7FF45E9D2D01304E1455706DD103'
$savedPythonPath = $env:PYTHONPATH
$localPythonPackages = Join-Path $repoRoot '.tools\python'

function Invoke-Native {
    param(
        [Parameter(Mandatory)]
        [string]$FilePath,
        [Parameter(ValueFromRemainingArguments)]
        [string[]]$Arguments
    )
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$FilePath failed with exit code $LASTEXITCODE"
    }
}

function Ensure-Patch {
    param(
        [Parameter(Mandatory)]
        [string]$Submodule,
        [Parameter(Mandatory)]
        [string]$Patch
    )
    $submodulePath = Join-Path $repoRoot $Submodule
    $patchPath = Join-Path $repoRoot $Patch
    $safeDirectory = $submodulePath.Replace('\', '/')
    $savedErrorPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    & git -c "safe.directory=$safeDirectory" -C $submodulePath `
        apply --ignore-whitespace --check $patchPath 2>$null
    $forwardCheck = $LASTEXITCODE
    $ErrorActionPreference = $savedErrorPreference
    if ($forwardCheck -eq 0) {
        Invoke-Native -FilePath git -Arguments @(
            '-c', "safe.directory=$safeDirectory",
            '-C', $submodulePath,
            'apply', '--ignore-whitespace', $patchPath)
        Write-Host "Applied $Patch"
        return
    }
    $ErrorActionPreference = 'Continue'
    & git -c "safe.directory=$safeDirectory" -C $submodulePath `
        apply --ignore-whitespace --reverse --check $patchPath 2>$null
    $reverseCheck = $LASTEXITCODE
    $ErrorActionPreference = $savedErrorPreference
    if ($reverseCheck -eq 0) {
        Write-Host "Already applied: $Patch"
        return
    }
    throw "Patch does not apply cleanly: $Patch"
}

function Remove-RepoDirectory {
    param(
        [Parameter(Mandatory)]
        [string]$Path
    )
    $resolvedRoot = [System.IO.Path]::GetFullPath($repoRoot)
    $resolvedTarget = [System.IO.Path]::GetFullPath($Path)
    if (-not $resolvedTarget.StartsWith(
            $resolvedRoot + [System.IO.Path]::DirectorySeparatorChar)) {
        throw "Refusing to remove path outside repository: $resolvedTarget"
    }
    if (Test-Path -LiteralPath $resolvedTarget -PathType Container) {
        Remove-Item -LiteralPath $resolvedTarget -Recurse -Force
    }
}

Push-Location $repoRoot
try {
    if (-not (Test-Path -LiteralPath $romAbsolute -PathType Leaf)) {
        throw "Missing ROM: $romAbsolute"
    }
    $actualSha256 = (
        Get-FileHash -LiteralPath $romAbsolute -Algorithm SHA256).Hash
    if ($actualSha256 -ne $expectedSha256) {
        throw "Unsupported ROM SHA-256 $actualSha256"
    }

    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
        throw 'CMake is not available on PATH.'
    }

    Write-Host '[1/7] Initializing submodules...'
    $safeRepo = $repoRoot.Replace('\', '/')
    Invoke-Native -FilePath git -Arguments @(
        '-c', "safe.directory=$safeRepo",
        '-c', 'core.longpaths=true',
        'submodule', 'update', '--init', '--recursive')

    Write-Host '[2/7] Applying runtime patches...'
    Ensure-Patch `
        -Submodule 'third_party/N64ModernRuntime' `
        -Patch 'patches/0001-n64modernruntime-sote-audio-fifo.patch'
    Ensure-Patch `
        -Submodule 'third_party/rt64' `
        -Patch 'patches/0002-rt64-sote-f3dbeta.patch'

    if ($Clean) {
        foreach ($target in @(
            (Join-Path $repoRoot 'build\n64recomp'),
            (Join-Path $repoRoot 'build\runtime')
        )) {
            Remove-RepoDirectory -Path $target
        }
    }

    $generatedGame = Join-Path $repoRoot `
        'generated\RecompiledFuncs\funcs_0.c'
    $generatedAudio = Join-Path $repoRoot 'generated\aspMain.cpp'
    $needsGeneration = $Regenerate -or
        -not (Test-Path -LiteralPath $generatedGame -PathType Leaf) -or
        -not (Test-Path -LiteralPath $generatedAudio -PathType Leaf)

    if ($needsGeneration) {
        $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
        if (-not $pythonCommand) {
            throw 'Python 3 is not available on PATH.'
        }
        $python = $pythonCommand.Source
        if (Test-Path -LiteralPath $localPythonPackages -PathType Container) {
            $env:PYTHONPATH = if ($savedPythonPath) {
                $localPythonPackages + [System.IO.Path]::PathSeparator +
                    $savedPythonPath
            }
            else {
                $localPythonPackages
            }
        }

        Write-Host '[3/7] Preparing the ROM-derived analysis image...'
        Invoke-Native -FilePath $python -Arguments @(
            'tools/prepare_recomp_rom.py', $romAbsolute)

        Write-Host '[4/7] Building N64Recomp tools...'
        Invoke-Native -FilePath cmake -Arguments @(
            '-S', 'third_party/N64Recomp',
            '-B', 'build/n64recomp',
            '-A', 'x64',
            '--log-level=WARNING',
            '-Wno-deprecated')
        Invoke-Native -FilePath cmake -Arguments @(
            '--build', 'build/n64recomp',
            '--config', 'Release',
            '--target', 'N64RecompCLI', 'RSPRecomp',
            '-j', '1')

        Write-Host '[5/7] Recompiling game and audio microcode...'
        # N64Recomp splits output by size. Remove the previous output set so a
        # later regeneration that needs fewer chunks cannot leave stale C
        # files in the runtime's source glob.
        Remove-RepoDirectory -Path (
            Join-Path $repoRoot 'generated\RecompiledFuncs')
        Invoke-Native -FilePath (
            Join-Path $repoRoot 'build\n64recomp\Release\N64Recomp.exe'
        ) -Arguments @('sote.toml')
        Invoke-Native -FilePath (
            Join-Path $repoRoot 'build\n64recomp\Release\RSPRecomp.exe'
        ) -Arguments @('config/aspMain.toml')
        Invoke-Native -FilePath $python -Arguments @(
            'tools/patch_rsp_audio.py')
    }
    else {
        Write-Host '[3-5/7] Generated sources already exist; skipping regeneration.'
        Write-Host '        Use -Regenerate to recreate them from the ROM.'
    }

    Write-Host '[6/7] Building the playable runtime...'
    Invoke-Native -FilePath cmake -Arguments @(
        '-S', '.',
        '-B', 'build/runtime',
        '-A', 'x64',
        '--log-level=WARNING',
        '-Wno-deprecated')
    Invoke-Native -FilePath cmake -Arguments @(
        '--build', 'build/runtime',
        '--config', 'Release',
        '-j', '1')

    Write-Host '[7/7] Creating the one-click play folder...'
    & (Join-Path $repoRoot 'tools\package_portable.ps1') `
        -RomPath $romAbsolute

    Write-Host ''
    Write-Host 'Built SotE_Recompiled\Shadows of the Empire.exe'
}
finally {
    if ($null -eq $savedPythonPath) {
        Remove-Item Env:PYTHONPATH -ErrorAction SilentlyContinue
    }
    else {
        $env:PYTHONPATH = $savedPythonPath
    }
    Pop-Location
}
