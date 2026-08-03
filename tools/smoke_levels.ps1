[CmdletBinding()]
param(
    [string]$ExePath = ".\build\runtime\Release\sote_recomp.exe",
    [string]$OutputDirectory = ".\build\diagnostics\level_smoke",
    [switch]$AllLevels,
    [int[]]$RequestedLevelIndex,
    [double]$DurationMinutes = 3,
    [switch]$Passive,
    [switch]$NoRefillLives,
    [switch]$ExpectNaturalGameOver,
    [ValidateSet("combined", "stick", "z", "jetpack")]
    [string]$InputMode = "combined"
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

# These levels cover SOTE's distinct gameplay modes and historically fragile
# paths. -AllLevels expands the same test to every Change Level entry.
$levelIndices = if ($RequestedLevelIndex.Count -ne 0) {
    $RequestedLevelIndex
} elseif ($AllLevels) {
    0..($levelNames.Count - 1)
} else {
    @(2, 4, 5, 9)
}

foreach ($index in $levelIndices) {
    if ($index -lt 0 -or $index -ge $levelNames.Count) {
        throw "LevelIndex must be between 0 and $($levelNames.Count - 1)."
    }
}
if ($DurationMinutes -le 0) {
    throw "DurationMinutes must be greater than zero."
}

$resolvedExe = (Resolve-Path -LiteralPath $ExePath).Path
$repoRoot = [System.IO.Path]::GetFullPath(
    (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)))
$sourceSave = Join-Path $repoRoot "SotE_Recompiled\saves\sote.us.v1.2.bin"
if (-not (Test-Path -LiteralPath $sourceSave -PathType Leaf)) {
    throw "Known-good Change Level save is missing: $sourceSave"
}
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$resolvedOutput = (Resolve-Path -LiteralPath $OutputDirectory).Path

function New-LevelInputScript {
    param(
        [int]$LevelIndex,
        [int]$ObservationStartVi,
        [int]$SmokeVi,
        [switch]$Passive,
        [string]$InputMode = "combined"
    )

    $pulses = [System.Collections.Generic.List[string]]::new()
    foreach ($entry in @(
        "120:5:start",
        "300:5:start",
        "660:5:start",
        # The selected profile menu opens with Start Game highlighted. Move
        # down once to Change Level, then confirm it.
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

    # Extra separated A presses harmlessly pass level-specific briefing and
    # difficulty screens. Later presses also retry after deaths/game overs.
    for ($vi = $selectionVi + 250; $vi -lt $SmokeVi - 30; $vi += 250) {
        $pulses.Add(("{0}:8:a" -f $vi))
    }

    $directions = @("stick_up", "stick_right", "stick_down", "stick_left")
    $cameraActions = @("cu", "cr", "cd", "cl")
    $directionIndex = 0
    if (-not $Passive -and $InputMode -eq "jetpack") {
        # Xizor's Palace and the sewers provide Dash's jetpack. C-left toggles
        # it and a sustained A press supplies thrust; the long input window
        # distinguishes working lift from an ordinary short jump.
        # This level's route becomes identifiable before its final intro has
        # released player input. Delay the toggle until Dash has been under
        # normal controller processing for several seconds.
        $pulses.Add(("{0}:8:cl" -f ($ObservationStartVi + 630)))
        for (
            $vi = $ObservationStartVi + 690;
            $vi -lt $SmokeVi - 360;
            $vi += 600) {
            $pulses.Add(("{0}:360:a+stick_up" -f $vi))
        }
    } elseif (-not $Passive) {
        for ($vi = $ObservationStartVi; $vi -lt $SmokeVi - 100; $vi += 300) {
            $direction = $directions[$directionIndex % $directions.Count]
            if ($InputMode -eq "combined") {
                # Exercise substantially more than passive movement. Overlap
                # locomotion/fire with short jump, attack/action, shoulder,
                # and camera/weapon-selection pulses. The exact meaning is
                # level-specific, which is useful: the same script reaches
                # distinct player, vehicle, turret, and spacecraft paths.
                $cameraAction =
                    $cameraActions[$directionIndex % $cameraActions.Count]
                $pulses.Add(("{0}:240:z+{1}" -f $vi, $direction))
                $pulses.Add(("{0}:10:a" -f ($vi + 45)))
                $pulses.Add(("{0}:16:b" -f ($vi + 90)))
                $pulses.Add(("{0}:16:r" -f ($vi + 135)))
                $pulses.Add(("{0}:16:{1}" -f ($vi + 180), $cameraAction))
                $pulses.Add(("{0}:16:l" -f ($vi + 215)))
            }
            else {
                $input = switch ($InputMode) {
                    "stick" { $direction }
                    "z" { "z" }
                }
                $pulses.Add(("{0}:180:{1}" -f $vi, $input))
            }
            ++$directionIndex
        }
    }

    return $pulses -join ","
}

$previousSmokeVis = $env:SOTE_SMOKE_VIS
$previousInputScript = $env:SOTE_INPUT_SCRIPT
$previousUnlock = $env:SOTE_DIAGNOSTIC_UNLOCK_LEVELS
$previousConfigPath = $env:SOTE_DIAGNOSTIC_CONFIG_PATH
$previousObservationStart =
    $env:SOTE_SMOKE_OBSERVATION_START_VI
$previousExpectedLevelIndex = $env:SOTE_EXPECT_LEVEL_INDEX
$previousExpectedLevelName = $env:SOTE_EXPECT_LEVEL_NAME
$previousRefillLives = $env:SOTE_SMOKE_REFILL_LIVES
$previousExpectNaturalGameOver =
    $env:SOTE_EXPECT_NATURAL_GAME_OVER
$previousTraceStalls = $env:SOTE_TRACE_STALLS
$previousTracePlayerState = $env:SOTE_TRACE_PLAYER_STATE
$failures = [System.Collections.Generic.List[string]]::new()

try {
    $env:SOTE_DIAGNOSTIC_UNLOCK_LEVELS = "1"
    $env:SOTE_SMOKE_REFILL_LIVES =
        if ($NoRefillLives -or $ExpectNaturalGameOver) {
            $null
        } else {
            "1"
        }
    $env:SOTE_EXPECT_NATURAL_GAME_OVER =
        if ($ExpectNaturalGameOver) { "1" } else { $null }
    $env:SOTE_TRACE_STALLS = "1"
    $env:SOTE_TRACE_PLAYER_STATE =
        if ($InputMode -eq "jetpack") {
            "1"
        } else {
            $previousTracePlayerState
        }

    foreach ($currentLevelIndex in $levelIndices) {
        $name = $levelNames[$currentLevelIndex]
        # Later Change Level entries require more menu pulses and therefore
        # start later. This per-level offset begins observation after loading
        # but before the unattended player has exhausted its first set of
        # lives.
        # The playable runtime advances at most one gameplay iteration per
        # 60 Hz VI. Allow enough real VI time for the title/profile, Change
        # Level, briefing, and difficulty screens before observation.
        $observationStartVi = 2700 + 70 * $currentLevelIndex
        $durationVis = [int][Math]::Ceiling($DurationMinutes * 60 * 60)
        $smokeVi = $observationStartVi + $durationVis
        $env:SOTE_SMOKE_VIS = [string]$smokeVi
        $env:SOTE_INPUT_SCRIPT =
            New-LevelInputScript `
                $currentLevelIndex $observationStartVi $smokeVi `
                -Passive:$Passive -InputMode $InputMode
        $env:SOTE_DIAGNOSTIC_CONFIG_PATH =
            Join-Path $resolvedOutput "config\$name"
        $env:SOTE_SMOKE_OBSERVATION_START_VI =
            [string]$observationStartVi
        $env:SOTE_EXPECT_LEVEL_INDEX = [string]$currentLevelIndex
        $env:SOTE_EXPECT_LEVEL_NAME = $name

        $saveDirectory = Join-Path `
            $env:SOTE_DIAGNOSTIC_CONFIG_PATH "saves"
        New-Item -ItemType Directory -Path $saveDirectory -Force |
            Out-Null
        Copy-Item -LiteralPath $sourceSave `
            -Destination (Join-Path $saveDirectory "sote.us.v1.2.bin") `
            -Force

        $stdoutPath = Join-Path $resolvedOutput "$name.stdout.log"
        $stderrPath = Join-Path $resolvedOutput "$name.stderr.log"
        Write-Host (
            "Testing {0} for {1:N2} minute(s) through VI {2}..." -f
            $name, $DurationMinutes, $smokeVi)

        # Direct process capture keeps native stderr intact even when
        # PowerShell's native-command error preference is set to Stop. A
        # crashing level therefore produces a complete stack and the suite
        # can continue to the next level.
        $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
        $startInfo.FileName = $resolvedExe
        $startInfo.Arguments = "--headless-smoke --muted"
        $startInfo.UseShellExecute = $false
        $startInfo.CreateNoWindow = $true
        $startInfo.RedirectStandardOutput = $true
        $startInfo.RedirectStandardError = $true
        $process = [System.Diagnostics.Process]::new()
        $process.StartInfo = $startInfo
        if (-not $process.Start()) {
            throw "Failed to start $resolvedExe"
        }
        $stdoutTask = $process.StandardOutput.ReadToEndAsync()
        $stderrTask = $process.StandardError.ReadToEndAsync()
        $process.WaitForExit()
        $exitCode = $process.ExitCode
        [System.IO.File]::WriteAllText(
            $stdoutPath, $stdoutTask.GetAwaiter().GetResult())
        [System.IO.File]::WriteAllText(
            $stderrPath, $stderrTask.GetAwaiter().GetResult())
        $process.Dispose()
        $stdout = Get-Content -LiteralPath $stdoutPath -Raw
        $stderr = Get-Content -LiteralPath $stderrPath -Raw
        $completed = $stdout -match (
            "\[sote\] smoke complete: VI={0} .*display_lists=([1-9][0-9]*)" -f
            $smokeVi
        )
        $muted = $stdout -match "\[sote\] audio output muted"
        $nonSilentAudio =
            $stdout -match "\[sote\] first non-silent audio buffer"
        $openedAudioDevice =
            $stdout -match "\[sote\] audio device opened"
        $observationStarted = $stdout -match ((
            "\[sote\]\[smoke\] LEVEL OBSERVATION START " +
            "expected_index={0} actual_index=[0-9-]+ route_ok=1 " +
            "expected_name={1} ") -f
            $currentLevelIndex, [regex]::Escape($name))
        $observationCompleted = $stdout -match ((
            "\[sote\]\[smoke\] LEVEL OBSERVATION COMPLETE " +
            "expected_index={0} expected_name={1} observed_vis={2} " +
            "display_lists_delta=([1-9][0-9]{{2,}}) " +
            "game_frames_delta=([1-9][0-9]{{2,}}) " +
            "display_stall_vis=([0-9]|[1-9][0-9])(?![0-9]) " +
            "motion_guards=0 rapid_life_losses=0 " +
            "player_delta_leaks=0 bike_delta_leaks=0") -f
            $currentLevelIndex, [regex]::Escape($name), $durationVis)
        $gameFrameMatch = [regex]::Match(
            $stdout,
            (((
                "\[sote\]\[smoke\] LEVEL OBSERVATION COMPLETE " +
                "expected_index={0} expected_name={1} observed_vis={2} " +
                "display_lists_delta=[0-9]+ game_frames_delta=([0-9]+)") -f
                $currentLevelIndex,
                [regex]::Escape($name),
                $durationVis)))
        $minimumGameFrames = [int][Math]::Floor($durationVis * 0.9)
        $gameplayCadence =
            $gameFrameMatch.Success -and
            [int64]$gameFrameMatch.Groups[1].Value -ge $minimumGameFrames
        $runtimeAnomaly = $stdout -match "\[sote\]\[anomaly\]"
        $naturalGameOverCompleted = $stdout -match (((
            "\[sote\]\[smoke\] NATURAL GAME OVER COMPLETE " +
            "expected_index={0} expected_name={1} VI=[0-9]+ " +
            "lives=-1 result=4 event=[0-9-]+") -f
            $currentLevelIndex, [regex]::Escape($name)))
        # Use the guard's commit records instead of observation-only failure
        # telemetry. A death can occur during a late level-loading event before
        # the observation window begins, and it still belongs in the four-life
        # sequence that leads to a legitimate game over.
        $lifeLossMatches = [regex]::Matches(
            $stdout,
            ((
                "\[sote\]\[lives\] committed life loss " +
                "source=[0-9A-F]+ VI=[0-9]+ event=[0-9-]+ " +
                "lives=(-?[0-9]+) object=[0-9A-F]+") ))
        $lifeLossSequence = (
            $lifeLossMatches | ForEach-Object {
                $_.Groups[1].Value
            }) -join ","
        $naturalLifeSequence =
            $lifeLossSequence -eq "3,2,1,0"
        $completionValid = if ($ExpectNaturalGameOver) {
            $naturalGameOverCompleted -and $naturalLifeSequence
        } else {
            $completed -and $observationCompleted
        }
        $actionCoverage =
            $Passive -or $InputMode -ne "combined" -or (
                $stdout -match "buttons=8000" -and
                $stdout -match "buttons=4000" -and
                $stdout -match "buttons=2000 stick=(80,0|-80,0|0,80|0,-80)" -and
                $stdout -match "buttons=0010" -and
                $stdout -match "buttons=0020" -and
                $stdout -match "buttons=000[1248]"
            )
        $jetpackLift = $true
        if ($InputMode -eq "jetpack") {
            $toggleState = [regex]::Match(
                $stdout,
                (
                    "\[sote\]\[player-state\] VI=[0-9]+ .*" +
                    "buttons=0002 .*pos=[-0-9.]+,[-0-9.]+,([-0-9.]+) " +
                    "velocity=[-0-9.]+ flags=([0-9A-F]+)"))
            $thrustStates = [regex]::Matches(
                $stdout,
                (
                    "\[sote\]\[player-state\] VI=[0-9]+ .*" +
                    "buttons=8000 .*pos=[-0-9.]+,[-0-9.]+,([-0-9.]+) "))
            if (-not $toggleState.Success -or
                $thrustStates.Count -eq 0) {
                $jetpackLift = $false
            } else {
                $toggleHeight = [double]::Parse(
                    $toggleState.Groups[1].Value,
                    [System.Globalization.CultureInfo]::InvariantCulture)
                $toggleFlags = [Convert]::ToUInt32(
                    $toggleState.Groups[2].Value, 16)
                $maximumThrustHeight = (
                    $thrustStates | ForEach-Object {
                        [double]::Parse(
                            $_.Groups[1].Value,
                            [System.Globalization.CultureInfo]::InvariantCulture)
                    } | Measure-Object -Maximum).Maximum
                $jetpackLift =
                    ($toggleFlags -band 0x0C000000) -eq 0x0C000000 -and
                    $maximumThrustHeight - $toggleHeight -ge 10.0
            }
        }

        if ($exitCode -ne 0 -or -not $completionValid -or
            -not $muted -or -not $nonSilentAudio -or $openedAudioDevice -or
            -not $observationStarted -or
            -not $gameplayCadence -or -not $jetpackLift -or
            $runtimeAnomaly -or -not $actionCoverage -or
            $stderr -match "unhandled exception|runtime error|invalid PI|unsupported RSP") {
            $failures.Add($name)
            Write-Host ("FAIL {0} (exit {1})" -f $name, $exitCode)
        } else {
            Write-Host ("PASS {0}" -f $name)
        }
    }
} finally {
    $env:SOTE_SMOKE_VIS = $previousSmokeVis
    $env:SOTE_INPUT_SCRIPT = $previousInputScript
    $env:SOTE_DIAGNOSTIC_UNLOCK_LEVELS = $previousUnlock
    $env:SOTE_DIAGNOSTIC_CONFIG_PATH = $previousConfigPath
    $env:SOTE_SMOKE_OBSERVATION_START_VI = $previousObservationStart
    $env:SOTE_EXPECT_LEVEL_INDEX = $previousExpectedLevelIndex
    $env:SOTE_EXPECT_LEVEL_NAME = $previousExpectedLevelName
    $env:SOTE_SMOKE_REFILL_LIVES = $previousRefillLives
    $env:SOTE_EXPECT_NATURAL_GAME_OVER =
        $previousExpectNaturalGameOver
    $env:SOTE_TRACE_STALLS = $previousTraceStalls
    $env:SOTE_TRACE_PLAYER_STATE = $previousTracePlayerState

    # select_rom stores a validated ROM copy under the registered config path.
    # Diagnostic saves and that ROM copy are disposable; retain only the
    # stdout/stderr evidence for each level.
    $diagnosticConfigRoot = [System.IO.Path]::GetFullPath(
        (Join-Path $resolvedOutput "config"))
    if (-not $diagnosticConfigRoot.StartsWith(
            $resolvedOutput + [System.IO.Path]::DirectorySeparatorChar,
            [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to clean outside diagnostic output: $diagnosticConfigRoot"
    }
    if (Test-Path -LiteralPath $diagnosticConfigRoot -PathType Container) {
        Remove-Item -LiteralPath $diagnosticConfigRoot -Recurse -Force
    }
}

if ($failures.Count -ne 0) {
    throw "Level smoke failures: $($failures -join ', ')"
}

Write-Host ("All {0} level smoke tests passed." -f $levelIndices.Count)
