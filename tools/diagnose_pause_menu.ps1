[CmdletBinding()]
param(
    [string]$ExePath = ".\build\runtime\Release\sote_recomp.exe",
    [string]$OutputDirectory = ".\build\diagnostics\pause_menu_probe",
    [int]$StopVi = 3500,
    [switch]$ExploreSubmenus,
    [switch]$ExploreCutscenes,
    [switch]$ExploreBindings,
    [switch]$BrowseBindings,
    [switch]$OriginalN64
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
$save = ".\SotE_Recompiled\saves\sote.us.v1.2.bin"
Copy-Item -LiteralPath $save -Destination (Join-Path $saveDirectory "sote.us.v1.2.bin")

# Game-process input only: reach Asteroid Field, then pause. Native
# RT64 capture writes the actual presented output without touching the desktop.
$inputScript = @(
    "120:5:start", "300:5:start", "660:5:start",
    "840:5:stick_down", "900:5:a", "1200:8:stick_down",
    "1260:8:stick_down", "1410:8:a",
    "1600:8:a", "1850:8:a", "2100:8:a", "2350:8:a", "2600:8:a"
) -join ","
if (-not ($ExploreBindings -or $BrowseBindings)) {
    $inputScript += ",3100:5:start,3250:8:stick_down,3310:8:a"
}
if ($ExploreSubmenus) {
    $inputScript += ",3380:8:r,3470:8:r"
}
if ($ExploreCutscenes) {
    $inputScript += ",3380:8:r,3470:8:stick_down,3530:8:stick_down,3590:8:stick_down,3650:8:stick_down,3710:8:stick_down,3770:8:stick_down,3830:8:stick_right,3890:8:stick_down,3950:8:a"
}

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
$startInfo.Environment["SOTE_INPUT_SCRIPT"] = $inputScript
if ($ExploreBindings -or $BrowseBindings) {
    $startInfo.Environment["SOTE_DIAGNOSTIC_BINDING_ROUTE"] = "1"
}
if ($BrowseBindings) {
    $startInfo.Environment["SOTE_DIAGNOSTIC_BINDING_BROWSE_ONLY"] = "1"
}
$startInfo.Environment["SOTE_SMOKE_VIS"] = [string]$StopVi
$startInfo.Environment["SOTE_VISIBLE_CAPTURE_PATH"] = Join-Path $output "frames"
$startInfo.Environment["SOTE_VISIBLE_CAPTURE_PRESENTS"] = if ($ExploreBindings -or $BrowseBindings) {
    "3400,3900,4300,4600"
} elseif ($ExploreCutscenes) {
    "3420,3800,3860,3970"
} elseif ($ExploreSubmenus) {
    "3120,3340,3420,3510,3590,3660"
} else { "3000,3080,3120,3200,3280,3340,3420" }
$startInfo.Environment["SOTE_DIAGNOSTIC_CAPTURE_SYNC"] = "1"
$startInfo.Environment["SOTE_TRACE_MENU_REVAMP"] = "1"
$startInfo.Environment["SOTE_TRACE_MENU"] = "1"

$optionsPath = Join-Path (Split-Path -Parent $exe) 'sote_options.json'
$hadOptions = Test-Path -LiteralPath $optionsPath
if ($hadOptions) { $savedOptions = [IO.File]::ReadAllBytes($optionsPath) }
try {
    if ($OriginalN64) {
        [IO.File]::WriteAllText($optionsPath, '{"pcCutscenes":false}')
    }
    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    if (-not $process.Start()) { throw "Could not start $exe" }
    $stdout = $process.StandardOutput.ReadToEndAsync()
    $stderr = $process.StandardError.ReadToEndAsync()
    $process.WaitForExit()
    [System.IO.File]::WriteAllText((Join-Path $output "stdout.log"), $stdout.GetAwaiter().GetResult())
    [System.IO.File]::WriteAllText((Join-Path $output "stderr.log"), $stderr.GetAwaiter().GetResult())
    $exit = $process.ExitCode
    $process.Dispose()
    Write-Host "Pause probe exit: $exit"
    Write-Host "Native captures and logs: $output"
    if ($exit -ne 0) { exit $exit }
} finally {
    if ($OriginalN64) {
        if ($hadOptions) { [IO.File]::WriteAllBytes($optionsPath, $savedOptions) }
        elseif (Test-Path -LiteralPath $optionsPath) {
            Remove-Item -LiteralPath $optionsPath
        }
    }
}
