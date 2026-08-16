param(
    [string]$Name = "baseline_visible_capture",
    [string]$ExePath = "./build/runtime/Release/sote_recomp.exe"
)

$ErrorActionPreference = "Stop"
$diag = Join-Path (Get-Location) "build/diagnostics/$Name"
Remove-Item -Recurse -Force $diag -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $diag | Out-Null

$levelIndex = 4
$observationStartVi = 2700 + 70 * $levelIndex
$smokeVi = $observationStartVi + [int][Math]::Ceiling(0.45 * 60 * 60)
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
for ($index = 0; $index -lt $levelIndex; ++$index) {
    $pulses.Add(("{0}:8:stick_down" -f (1200 + 60 * $index)))
}
$selectionVi = 1290 + 60 * $levelIndex
$pulses.Add(("{0}:8:a" -f $selectionVi))
for ($vi = $selectionVi + 250; $vi -lt $smokeVi - 30; $vi += 250) {
    $pulses.Add(("{0}:8:a" -f $vi))
}
$directions = @("stick_up", "stick_right", "stick_down", "stick_left")
$cameraActions = @("cu", "cr", "cd", "cl")
$directionIndex = 0
for ($vi = $observationStartVi; $vi -lt $smokeVi - 100; $vi += 300) {
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
$inputScript = $pulses -join ","

$ids = (2400..3200 | Where-Object { ($_ - 2400) % 20 -eq 0 }) +
    @(2025, 2403, 2590, 2733, 2884, 3074)
$ids = $ids | Sort-Object -Unique

$env:SOTE_VISIBLE_CAPTURE_PATH = Join-Path $diag "visible_frames"
$env:SOTE_VISIBLE_CAPTURE_PRESENTS = $ids -join ","
$env:SOTE_DIAGNOSTIC_CAPTURE_SYNC = "1"
$env:SOTE_DIAGNOSTIC_UNLOCK_LEVELS = "1"
$env:SOTE_SMOKE_REFILL_LIVES = "1"
$env:SOTE_SMOKE_OBSERVATION_START_VI = [string]$observationStartVi
$env:SOTE_EXPECT_LEVEL_INDEX = [string]$levelIndex
$env:SOTE_EXPECT_LEVEL_NAME = "gall_spaceport"

$configPath = Join-Path $diag "capture_config"
$saveDirectory = Join-Path $configPath "saves"
New-Item -ItemType Directory -Force $saveDirectory | Out-Null
Copy-Item -LiteralPath "SotE_Recompiled/saves/sote.us.v1.2.bin" `
    -Destination (Join-Path $saveDirectory "sote.us.v1.2.bin") `
    -Force

python ./tools/capture_offscreen.py `
    --exe $ExePath `
    --capture-vi $smokeVi `
    --smoke-vi $smokeVi `
    --input-script $inputScript `
    --output (Join-Path $diag "window_capture.png") `
    --stdout (Join-Path $diag "gall_spaceport.stdout.log") `
    --stderr (Join-Path $diag "gall_spaceport.stderr.log")
