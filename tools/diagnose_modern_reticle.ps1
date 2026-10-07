[CmdletBinding()]
param(
    [string]$ExePath = ".\build\runtime\Release\sote_recomp.exe",
    [string]$OutputDirectory = ".\build\diagnostics\modern_reticle_probe",
    [switch]$MouseAim,
    [switch]$Idle,
    [switch]$OriginalN64,
    [string]$GameplayInput = '',
    [string]$PhysicalMouse = '',
    [string]$PhysicalPad = '',
    [int]$StopVi = 3350,
    [string]$CapturePresents = '2800,3000,3100',
    [switch]$TracePlayer
)

$ErrorActionPreference = "Stop"
$exe = (Resolve-Path -LiteralPath $ExePath).Path
$output = [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputDirectory))
if (Test-Path -LiteralPath $output) {
    throw "Choose a new output directory; this one already exists: $output"
}
New-Item -ItemType Directory -Path $output -Force | Out-Null
$saveDirectory = Join-Path $output "config\saves"
New-Item -ItemType Directory -Path $saveDirectory -Force | Out-Null
Copy-Item -LiteralPath ".\SotE_Recompiled\saves\sote.us.v1.2.bin" `
    -Destination (Join-Path $saveDirectory "sote.us.v1.2.bin")

# Select Escape from Echo Base (the first on-foot level), clear its briefing,
# and use the game's process-local Modern input diagnostic. No host input.
$inputScript = @(
    "120:5:start", "300:5:start", "660:5:start", "840:5:stick_down",
    "900:5:a", "1200:8:stick_down", "1350:8:a", "1600:8:a",
    "1850:8:a", "2000:8:start", "2200:8:b", "2300:8:start",
    "2490:8:a"
) -join ","
if ($GameplayInput) { $inputScript += ",$GameplayInput" }
$startInfo = [System.Diagnostics.ProcessStartInfo]::new()
$startInfo.FileName = $exe
$startInfo.Arguments = "--frontend-smoke --muted"
$startInfo.UseShellExecute = $false
$startInfo.CreateNoWindow = $true
$startInfo.RedirectStandardOutput = $true
$startInfo.RedirectStandardError = $true
$startInfo.Environment["SOTE_DIAGNOSTIC_OFFSCREEN"] = "1"
$startInfo.Environment["SOTE_DIAGNOSTIC_CONFIG_PATH"] = Join-Path $output "config"
$startInfo.Environment["SOTE_DIAGNOSTIC_UNLOCK_LEVELS"] = "1"
$startInfo.Environment["SOTE_SMOKE_REFILL_LIVES"] = "1"
if ($PhysicalPad) {
    $startInfo.Environment["SOTE_DIAGNOSTIC_PHYSICAL_PAD"] = $PhysicalPad
    $startInfo.Environment["SOTE_TRACE_INPUT"] = "1"
    $startInfo.Environment["SOTE_TRACE_GUEST_INPUT"] = "1"
    $startInfo.Environment["SOTE_TRACE_MODERN_CONTROLS"] = "1"
} elseif ($PhysicalMouse) {
    $startInfo.Environment["SOTE_DIAGNOSTIC_PHYSICAL_MOUSE"] = $PhysicalMouse
} elseif ($MouseAim) {
    $startInfo.Environment["SOTE_TEST_MOUSE_AIM"] = "1"
} elseif ($Idle) {
    $startInfo.Environment["SOTE_TEST_MODERN_IDLE"] = "1"
} else {
    $startInfo.Environment["SOTE_TEST_MODERN_INPUT"] = "1"
}
$startInfo.Environment["SOTE_TRACE_MODERN_AIM"] = "1"
if ($TracePlayer) {
    $startInfo.Environment["SOTE_TRACE_PLAYER_STATE"] = "1"
}
$startInfo.Environment["SOTE_INPUT_SCRIPT"] = $inputScript
$startInfo.Environment["SOTE_SMOKE_VIS"] = [string]$StopVi
$startInfo.Environment["SOTE_VISIBLE_CAPTURE_PATH"] = Join-Path $output "frames"
$startInfo.Environment["SOTE_VISIBLE_CAPTURE_PRESENTS"] = $CapturePresents
$startInfo.Environment["SOTE_DIAGNOSTIC_CAPTURE_SYNC"] = "1"

$schemePath = Join-Path (Split-Path -Parent $exe) 'sote_controls.json'
$hadScheme = Test-Path -LiteralPath $schemePath
if ($hadScheme) { $savedScheme = [IO.File]::ReadAllBytes($schemePath) }
$optionsPath = Join-Path (Split-Path -Parent $exe) 'sote_options.json'
$hadOptions = Test-Path -LiteralPath $optionsPath
if ($hadOptions) { $savedOptions = [IO.File]::ReadAllBytes($optionsPath) }
try {
    [IO.File]::WriteAllText(
        $schemePath, '{"on_foot":"Modern","bike":"Modern"}')
    if ($OriginalN64) {
        [IO.File]::WriteAllText($optionsPath, '{"pcCutscenes":false}')
    }
    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    if (-not $process.Start()) { throw "Could not start $exe" }
    $stdout = $process.StandardOutput.ReadToEndAsync()
    $stderr = $process.StandardError.ReadToEndAsync()
    $process.WaitForExit()
    [IO.File]::WriteAllText((Join-Path $output "stdout.log"), $stdout.GetAwaiter().GetResult())
    [IO.File]::WriteAllText((Join-Path $output "stderr.log"), $stderr.GetAwaiter().GetResult())
    $exit = $process.ExitCode
    $process.Dispose()
    Write-Host "Modern reticle probe exit: $exit"
    Write-Host "Native captures and logs: $output"
    if ($exit -ne 0) { exit $exit }
} finally {
    if ($hadScheme) { [IO.File]::WriteAllBytes($schemePath, $savedScheme) }
    elseif (Test-Path -LiteralPath $schemePath) {
        Remove-Item -LiteralPath $schemePath
    }
    if ($OriginalN64) {
        if ($hadOptions) { [IO.File]::WriteAllBytes($optionsPath, $savedOptions) }
        elseif (Test-Path -LiteralPath $optionsPath) {
            Remove-Item -LiteralPath $optionsPath
        }
    }
}
