[CmdletBinding()]
param(
    [string]$ExePath = '.\build\runtime\Release\sote_recomp.exe',
    [string]$OutputDirectory = '.\build\diagnostics\modern_bike_probe',
    [string]$Throttle = 'sweep',
    [ValidateRange(0.0, 1.0)][double]$Brake = 0.0,
    [switch]$Straight,
    [ValidateRange(-127, 127)][int]$StickY = 0
)

$ErrorActionPreference = 'Stop'
$exe = (Resolve-Path -LiteralPath $ExePath).Path
$output = [IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputDirectory))
if (Test-Path -LiteralPath $output) { throw "Output exists: $output" }
New-Item -ItemType Directory -Path $output -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $output 'config\saves') -Force | Out-Null
Copy-Item -LiteralPath '.\SotE_Recompiled\saves\sote.us.v1.2.bin' `
    -Destination (Join-Path $output 'config\saves\sote.us.v1.2.bin')

# The game reads scheme settings beside its executable. Restore that file
# exactly after this process-local diagnostic run.
$schemes = Join-Path (Split-Path -Parent $exe) 'sote_controls.json'
$hadSchemes = Test-Path -LiteralPath $schemes
if ($hadSchemes) { $savedSchemes = [IO.File]::ReadAllBytes($schemes) }
$inputScript = @(
    '120:5:start', '300:5:start', '660:5:start',
    '840:5:stick_down', '900:5:a',
    '1050:8:stick_down', '1110:8:stick_down',
    '1170:8:stick_down', '1230:8:stick_down',
    '1290:8:stick_down', '1410:8:a',
    '1600:8:a', '1850:8:a', '2100:8:a'
) -join ','
if (-not $Straight) {
    $inputScript += ',2300:60:stick_x=40,2600:60:stick_x=80,2800:30:a'
}
if ($StickY -ne 0) {
    $inputScript += ",2200:90:stick_y=$StickY"
}
try {
    [IO.File]::WriteAllText($schemes, '{"on_foot":"Modern","bike":"Modern"}')
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $exe
    $info.Arguments = '--frontend-smoke --muted'
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.Environment['SOTE_DIAGNOSTIC_OFFSCREEN'] = '1'
    $info.Environment['SOTE_DIAGNOSTIC_CONFIG_PATH'] = Join-Path $output 'config'
    $info.Environment['SOTE_DIAGNOSTIC_UNLOCK_LEVELS'] = '1'
    $info.Environment['SOTE_SMOKE_REFILL_LIVES'] = '1'
    $info.Environment['SOTE_FORCE_BIKE_THROTTLE'] = $Throttle
    $info.Environment['SOTE_FORCE_BIKE_BRAKE'] = $Brake.ToString(
        [Globalization.CultureInfo]::InvariantCulture)
    $info.Environment['SOTE_TRACE_BIKE_STATE'] = '1'
    $info.Environment['SOTE_INPUT_SCRIPT'] = $inputScript
    $info.Environment['SOTE_SMOKE_VIS'] = '3050'
    $info.Environment['SOTE_VISIBLE_CAPTURE_PATH'] = Join-Path $output 'frames'
    $info.Environment['SOTE_VISIBLE_CAPTURE_PRESENTS'] = '1900,2300,2700,3000'
    $info.Environment['SOTE_DIAGNOSTIC_CAPTURE_SYNC'] = '1'
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $info
    if (-not $process.Start()) { throw 'Could not start game' }
    $stdout = $process.StandardOutput.ReadToEndAsync()
    $stderr = $process.StandardError.ReadToEndAsync()
    $process.WaitForExit()
    [IO.File]::WriteAllText((Join-Path $output 'stdout.log'), $stdout.GetAwaiter().GetResult())
    [IO.File]::WriteAllText((Join-Path $output 'stderr.log'), $stderr.GetAwaiter().GetResult())
    $exit = $process.ExitCode
    $process.Dispose()
    Write-Host "Bike probe exit: $exit"
    Write-Host "Native captures and logs: $output"
    if ($exit -ne 0) { exit $exit }
} finally {
    if ($hadSchemes) { [IO.File]::WriteAllBytes($schemes, $savedSchemes) }
    elseif (Test-Path -LiteralPath $schemes) { Remove-Item -LiteralPath $schemes }
}
