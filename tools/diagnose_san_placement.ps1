[CmdletBinding()]
param(
    [string]$ExePath = '.\build\candidate-controls-menu\Shadows of the Empire.exe',
    [string]$OutputDirectory = '.\build\diagnostics\san_level_placement_probe',
    [int]$StopVi = 3500,
    [ValidateRange(0, 9)][int]$LevelIndex = 1,
    [switch]$OriginalN64,
    [switch]$AudioProbe,
    [ValidateRange(0, 20000)][int]$InitialSkipVi = 600,
    [string]$PreviewMovie = '',
    [switch]$TraceFinal,
    [switch]$TracePlayer,
    [switch]$TraceAim,
    [switch]$TraceGallBoss,
    [switch]$DirectEventOnly,
    [string]$EventJumps = '',
    [string]$Teleport = '',
    [string]$PhysicalPad = '',
    [string]$ExtraInput = '',
    [int]$RdramSnapshotStartVi = 0,
    [int]$RdramSnapshotPeriodVi = 300,
    [string]$CapturePresents = '1800,2200,2600,3000,3400,3600,4200,4600,5000'
)

$ErrorActionPreference = 'Stop'
$exe = (Resolve-Path -LiteralPath $ExePath).Path
$output = [IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputDirectory))
if (Test-Path -LiteralPath $output) { throw "Output exists: $output" }
New-Item -ItemType Directory -Path (Join-Path $output 'config\saves') -Force | Out-Null
Copy-Item -LiteralPath '.\SotE_Recompiled\saves\sote.us.v1.2.bin' `
    -Destination (Join-Path $output 'config\saves\sote.us.v1.2.bin')

# Select Escape from Echo Base through the game's own menus. Bypass only the
# opening SAN pair so the level movie can be observed at a bounded VI count.
$inputItems = @('120:5:start', '300:5:start', '660:5:start')
if (-not $DirectEventOnly) {
    $inputItems += '840:5:stick_down', '900:5:a'
    for ($index = 0; $index -lt $LevelIndex; ++$index) {
        $inputItems += "$(1050 + 60 * $index):8:stick_down"
    }
    $selectVi = [Math]::Max(1350, 1170 + 60 * $LevelIndex)
    $inputItems += "${selectVi}:8:a"
}
if ($ExtraInput) { $inputItems += $ExtraInput }
$inputs = $inputItems -join ','
$info = [Diagnostics.ProcessStartInfo]::new()
$info.FileName = $exe
$info.Arguments = if ($AudioProbe) { '--frontend-smoke' } else { '--frontend-smoke --muted' }
$info.UseShellExecute = $false
$info.CreateNoWindow = $true
$info.RedirectStandardOutput = $true
$info.RedirectStandardError = $true
$info.Environment['SOTE_DIAGNOSTIC_OFFSCREEN'] = '1'
if ($AudioProbe) { $info.Environment['SDL_AUDIODRIVER'] = 'dummy' }
$info.Environment['SOTE_DIAGNOSTIC_CONFIG_PATH'] = Join-Path $output 'config'
$info.Environment['SOTE_DIAGNOSTIC_UNLOCK_LEVELS'] = '1'
$info.Environment['SOTE_DIAGNOSTIC_SKIP_SAN_STARTUP'] = '1'
if ($PreviewMovie) { $info.Environment['SOTE_SAN_PREVIEW'] = $PreviewMovie }
if ($TraceFinal) { $info.Environment['SOTE_TRACE_FINAL_EVENT'] = '1' }
if ($TracePlayer) { $info.Environment['SOTE_TRACE_PLAYER_STATE'] = '1' }
if ($TraceAim) { $info.Environment['SOTE_TRACE_MODERN_AIM'] = '1' }
if ($TraceGallBoss) { $info.Environment['SOTE_TRACE_GALL_BOSS'] = '1' }
if ($InitialSkipVi -gt 0) {
    $info.Environment['SOTE_SAN_TEST_SKIP_VI'] = [string]$InitialSkipVi
}
if ($EventJumps) {
    $info.Environment['SOTE_DIAGNOSTIC_EVENT_JUMPS'] = $EventJumps
}
if ($Teleport) {
    $info.Environment['SOTE_DIAGNOSTIC_TELEPORT'] = $Teleport
}
if ($PhysicalPad) {
    $info.Environment['SOTE_DIAGNOSTIC_PHYSICAL_PAD'] = $PhysicalPad
    $info.Environment['SOTE_TRACE_INPUT'] = '1'
    $info.Environment['SOTE_TRACE_GUEST_INPUT'] = '1'
}
if ($RdramSnapshotStartVi -gt 0) {
    $memoryOutput = Join-Path $output 'rdram'
    New-Item -ItemType Directory -Path $memoryOutput -Force | Out-Null
    $info.Environment['SOTE_DUMP_RDRAM_EVERY'] = "${RdramSnapshotStartVi}:${RdramSnapshotPeriodVi}:${memoryOutput}"
}
$info.Environment['SOTE_INPUT_SCRIPT'] = $inputs
$info.Environment['SOTE_SMOKE_VIS'] = [string]$StopVi
$info.Environment['SOTE_VISIBLE_CAPTURE_PATH'] = Join-Path $output 'frames'
$info.Environment['SOTE_VISIBLE_CAPTURE_PRESENTS'] = $CapturePresents
$info.Environment['SOTE_DIAGNOSTIC_CAPTURE_SYNC'] = '1'
$info.Environment['SOTE_TRACE_MENU'] = '1'

$optionsPath = Join-Path (Split-Path -Parent $exe) 'sote_options.json'
$hadOptions = Test-Path -LiteralPath $optionsPath
if ($hadOptions) { $savedOptions = [IO.File]::ReadAllBytes($optionsPath) }
$controlsPath = Join-Path (Split-Path -Parent $exe) 'sote_controls.json'
$hadControls = Test-Path -LiteralPath $controlsPath
if ($hadControls) { $savedControls = [IO.File]::ReadAllBytes($controlsPath) }
try {
    if ($OriginalN64) {
        [IO.File]::WriteAllText($optionsPath, '{"pcCutscenes":false}')
    }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $info
    if (-not $process.Start()) { throw 'Could not start game' }
    $stdout = $process.StandardOutput.ReadToEndAsync()
    $stderr = $process.StandardError.ReadToEndAsync()
    $process.WaitForExit()
    [IO.File]::WriteAllText((Join-Path $output 'stdout.log'), $stdout.GetAwaiter().GetResult())
    [IO.File]::WriteAllText((Join-Path $output 'stderr.log'), $stderr.GetAwaiter().GetResult())
    if (Test-Path -LiteralPath $controlsPath) {
        [IO.File]::WriteAllBytes(
            (Join-Path $output 'controls_after.json'),
            [IO.File]::ReadAllBytes($controlsPath))
    }
    $exit = $process.ExitCode
    $process.Dispose()
    Write-Host "SAN placement probe exit: $exit"
    Write-Host "Native captures and logs: $output"
    if ($exit -ne 0) { exit $exit }
} finally {
    if ($hadControls) { [IO.File]::WriteAllBytes($controlsPath, $savedControls) }
    elseif (Test-Path -LiteralPath $controlsPath) {
        Remove-Item -LiteralPath $controlsPath
    }
    if ($OriginalN64) {
        if ($hadOptions) { [IO.File]::WriteAllBytes($optionsPath, $savedOptions) }
        elseif (Test-Path -LiteralPath $optionsPath) {
            Remove-Item -LiteralPath $optionsPath
        }
    }
}
