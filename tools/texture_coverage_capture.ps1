[CmdletBinding()]
param(
    [string]$ExePath = ".\SotE_Recompiled\Shadows of the Empire.exe",
    [string]$OutputDirectory = ".\build\diagnostics\texture_coverage",
    [int[]]$RequestedLevelIndex,
    [switch]$AllLevels,
    [switch]$IncludeUi,
    [switch]$UiOnly,
    [switch]$DumpTextures,
    [double]$DurationMinutes = 0.45,
    [string]$TexturePackPath = ""
)

$ErrorActionPreference = "Stop"

$levelNames = @(
    "battle_of_hoth",
    "escape_from_echo_base",
    "asteroid_field",
    "ord_mantell_junkyard",
    "gall_spaceport",
    "mos_eisley_beggars_canyon",
    "imperial_freighter",
    "xizors_palace",
    "sewers_of_imperial_city",
    "skyhook_battle"
)

$levelIndices = if ($RequestedLevelIndex.Count -ne 0) {
    $RequestedLevelIndex
} elseif ($AllLevels) {
    0..($levelNames.Count - 1)
} else {
    @(4)
}

foreach ($index in $levelIndices) {
    if ($index -lt 0 -or $index -ge $levelNames.Count) {
        throw "LevelIndex must be between 0 and $($levelNames.Count - 1)."
    }
}

$repoRoot = [System.IO.Path]::GetFullPath(
    (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)))
$resolvedExe = (Resolve-Path -LiteralPath $ExePath).Path
$resolvedOutput = [System.IO.Path]::GetFullPath(
    (Join-Path (Get-Location) $OutputDirectory))
New-Item -ItemType Directory -Path $resolvedOutput -Force | Out-Null

$sourceSave = Join-Path $repoRoot "SotE_Recompiled\saves\sote.us.v1.2.bin"
if (-not (Test-Path -LiteralPath $sourceSave -PathType Leaf)) {
    throw "Known-good Change Level save is missing: $sourceSave"
}

function New-LevelInputScript {
    param(
        [int]$LevelIndex,
        [int]$ObservationStartVi,
        [int]$SmokeVi
    )

    $pulses = [System.Collections.Generic.List[string]]::new()
    foreach ($entry in @(
        "120:5:start",
        "300:5:start",
        "660:5:start",
        "840:5:stick_down",
        "900:5:a"
    )) {
        $pulses.Add($entry)
    }

    for ($index = 0; $index -lt $LevelIndex; ++$index) {
        $pulses.Add(("{0}:8:stick_down" -f (1200 + 60 * $index)))
    }

    $selectionVi = 1290 + 60 * $LevelIndex
    $pulses.Add(("{0}:8:a" -f $selectionVi))
    for ($vi = $selectionVi + 250; $vi -lt $SmokeVi - 30; $vi += 250) {
        $pulses.Add(("{0}:8:a" -f $vi))
    }

    $directions = @("stick_up", "stick_right", "stick_down", "stick_left")
    $cameraActions = @("cu", "cr", "cd", "cl")
    $directionIndex = 0
    for ($vi = $ObservationStartVi; $vi -lt $SmokeVi - 100; $vi += 300) {
        $direction = $directions[$directionIndex % $directions.Count]
        $cameraAction = $cameraActions[$directionIndex % $cameraActions.Count]
        $pulses.Add(("{0}:240:z+{1}" -f $vi, $direction))
        $pulses.Add(("{0}:10:a" -f ($vi + 45)))
        $pulses.Add(("{0}:16:b" -f ($vi + 90)))
        $pulses.Add(("{0}:16:r" -f ($vi + 135)))
        $pulses.Add(("{0}:16:{1}" -f ($vi + 180), $cameraAction))
        $pulses.Add(("{0}:16:l" -f ($vi + 215)))
        ++$directionIndex
    }

    return $pulses -join ","
}

function Invoke-CoverageRun {
    param(
        [string]$Name,
        [int]$SmokeVi,
        [string]$InputScript,
        [string]$ExpectedLevelIndex = "",
        [string]$ExpectedLevelName = "",
        [int]$ObservationStartVi = 0,
        [int[]]$CaptureVis = @()
    )

    $diag = Join-Path $resolvedOutput $Name
    if (Test-Path -LiteralPath $diag) {
        Remove-Item -LiteralPath $diag -Recurse -Force
    }
    New-Item -ItemType Directory -Path $diag -Force | Out-Null

    $configPath = Join-Path $diag "capture_config"
    $saveDirectory = Join-Path $configPath "saves"
    New-Item -ItemType Directory -Path $saveDirectory -Force | Out-Null
    Copy-Item -LiteralPath $sourceSave `
        -Destination (Join-Path $saveDirectory "sote.us.v1.2.bin") `
        -Force

    $previousHashLog = $env:SOTE_TEXTURE_HASH_LOG
    $previousVisiblePath = $env:SOTE_VISIBLE_CAPTURE_PATH
    $previousVisiblePresents = $env:SOTE_VISIBLE_CAPTURE_PRESENTS
    $previousCaptureSync = $env:SOTE_DIAGNOSTIC_CAPTURE_SYNC
    $previousUnlock = $env:SOTE_DIAGNOSTIC_UNLOCK_LEVELS
    $previousRefill = $env:SOTE_SMOKE_REFILL_LIVES
    $previousObservationStart = $env:SOTE_SMOKE_OBSERVATION_START_VI
    $previousExpectedIndex = $env:SOTE_EXPECT_LEVEL_INDEX
    $previousExpectedName = $env:SOTE_EXPECT_LEVEL_NAME
    $previousTexturePack = $env:SOTE_TEXTURE_PACK_PATH
    $previousTextureDump = $env:SOTE_TEXTURE_DUMP_PATH

    try {
        $env:SOTE_TEXTURE_HASH_LOG = Join-Path $diag "texture_hashes.tsv"
        if ($DumpTextures) {
            $env:SOTE_TEXTURE_DUMP_PATH = Join-Path $diag "texture_dumps"
        } else {
            $env:SOTE_TEXTURE_DUMP_PATH = $null
        }
        $env:SOTE_VISIBLE_CAPTURE_PATH = Join-Path $diag "visible_frames"
        if ($CaptureVis.Count -ne 0) {
            $env:SOTE_VISIBLE_CAPTURE_PRESENTS = ($CaptureVis | Sort-Object -Unique) -join ","
        } else {
            $env:SOTE_VISIBLE_CAPTURE_PRESENTS = $null
        }
        $env:SOTE_DIAGNOSTIC_CAPTURE_SYNC = "1"
        $env:SOTE_DIAGNOSTIC_UNLOCK_LEVELS = "1"
        $env:SOTE_SMOKE_REFILL_LIVES = "1"
        $env:SOTE_SMOKE_OBSERVATION_START_VI = [string]$ObservationStartVi
        $env:SOTE_EXPECT_LEVEL_INDEX = $ExpectedLevelIndex
        $env:SOTE_EXPECT_LEVEL_NAME = $ExpectedLevelName
        if ($TexturePackPath -ne "") {
            $env:SOTE_TEXTURE_PACK_PATH = $TexturePackPath
        } else {
            $env:SOTE_TEXTURE_PACK_PATH = $null
        }

        $captureArgs = @(
            "./tools/capture_offscreen.py",
            "--exe", $resolvedExe,
            "--capture-vi", ([string]($SmokeVi - 1)),
            "--smoke-vi", ([string]$SmokeVi),
            "--input-script", $InputScript,
            "--output", (Join-Path $diag "window_capture.png"),
            "--stdout", (Join-Path $diag "$Name.stdout.log"),
            "--stderr", (Join-Path $diag "$Name.stderr.log")
        )
        python @captureArgs
    } finally {
        $env:SOTE_TEXTURE_HASH_LOG = $previousHashLog
        $env:SOTE_VISIBLE_CAPTURE_PATH = $previousVisiblePath
        $env:SOTE_VISIBLE_CAPTURE_PRESENTS = $previousVisiblePresents
        $env:SOTE_DIAGNOSTIC_CAPTURE_SYNC = $previousCaptureSync
        $env:SOTE_DIAGNOSTIC_UNLOCK_LEVELS = $previousUnlock
        $env:SOTE_SMOKE_REFILL_LIVES = $previousRefill
        $env:SOTE_SMOKE_OBSERVATION_START_VI = $previousObservationStart
        $env:SOTE_EXPECT_LEVEL_INDEX = $previousExpectedIndex
        $env:SOTE_EXPECT_LEVEL_NAME = $previousExpectedName
        $env:SOTE_TEXTURE_PACK_PATH = $previousTexturePack
        $env:SOTE_TEXTURE_DUMP_PATH = $previousTextureDump
    }

    $hashCount = if (Test-Path -LiteralPath (Join-Path $diag "texture_hashes.tsv")) {
        (Get-Content -LiteralPath (Join-Path $diag "texture_hashes.tsv") |
            Where-Object { $_ -and -not $_.StartsWith("hash`t") }).Count
    } else {
        0
    }
    Write-Host ("{0}: {1} unique texture hash(es)" -f $Name, $hashCount)
}

if ($UiOnly) {
    $IncludeUi = $true
}

if ($IncludeUi) {
    $uiPulses = @(
        "120:5:start",
        "300:5:start",
        "660:5:start",
        "840:5:stick_down",
        "900:5:a",
        "1080:8:b",
        "1260:8:stick_down",
        "1320:8:stick_down",
        "1380:8:a",
        "1620:8:b",
        "1800:8:start"
    ) -join ","
    Invoke-CoverageRun `
        -Name "ui_frontend" `
        -SmokeVi 2400 `
        -InputScript $uiPulses `
        -CaptureVis @(240, 600, 900, 1320, 1800, 2300)
}

if (-not $UiOnly) {
    foreach ($currentLevelIndex in $levelIndices) {
        $name = $levelNames[$currentLevelIndex]
        $observationStartVi = 2700 + 70 * $currentLevelIndex
        $durationVis = [int][Math]::Ceiling($DurationMinutes * 60 * 60)
        $smokeVi = $observationStartVi + $durationVis
        $captureVis = @()
        for ($vi = $observationStartVi; $vi -lt $smokeVi; $vi += 300) {
            $captureVis += $vi
        }
        $captureVis += ($smokeVi - 1)

        Invoke-CoverageRun `
            -Name $name `
            -SmokeVi $smokeVi `
            -InputScript (New-LevelInputScript $currentLevelIndex $observationStartVi $smokeVi) `
            -ExpectedLevelIndex ([string]$currentLevelIndex) `
            -ExpectedLevelName $name `
            -ObservationStartVi $observationStartVi `
            -CaptureVis $captureVis
    }
}

Write-Host "Texture coverage written to $resolvedOutput"
