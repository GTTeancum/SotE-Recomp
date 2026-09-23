<#
.SYNOPSIS
    Build a local decoded cache for PC SAN/SMUSH cutscene files.

.DESCRIPTION
    Uses the user's local FFmpeg to decode root-level .SAN files into a simple
    cache format under the runnable Sdata folder:

      Sdata/SAN_CACHE/<movie>/frames.rgba
      Sdata/SAN_CACHE/<movie>/audio.pcm
      Sdata/SAN_CACHE/<movie>/metadata.tsv

    The cache is generated from user-provided movie files and should not be
    committed. It is a bridge format for runtime playback work without
    embedding FFmpeg or third-party SAN decoder source into the executable.
#>
[CmdletBinding()]
param(
    [string]$InputDirectory = 'SotE_Recompiled\Sdata',
    [string]$CacheDirectory = '',
    [string[]]$MovieName = @(),
    [switch]$FirstFrameOnly,
    [switch]$AudioOnly
)

$ErrorActionPreference = 'Stop'
if ($AudioOnly -and $FirstFrameOnly) {
    throw '-AudioOnly and -FirstFrameOnly cannot be used together.'
}
$repoRoot = [System.IO.Path]::GetFullPath(
    (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)))
$inputAbsolute = if ([System.IO.Path]::IsPathRooted($InputDirectory)) {
    [System.IO.Path]::GetFullPath($InputDirectory)
}
else {
    [System.IO.Path]::GetFullPath((Join-Path $repoRoot $InputDirectory))
}
$cacheAbsolute = if ($CacheDirectory) {
    if ([System.IO.Path]::IsPathRooted($CacheDirectory)) {
        [System.IO.Path]::GetFullPath($CacheDirectory)
    }
    else {
        [System.IO.Path]::GetFullPath((Join-Path $repoRoot $CacheDirectory))
    }
}
else {
    Join-Path $inputAbsolute 'SAN_CACHE'
}

if (-not (Get-Command ffmpeg -ErrorAction SilentlyContinue)) {
    throw 'ffmpeg is not available on PATH.'
}
if (-not (Get-Command ffprobe -ErrorAction SilentlyContinue)) {
    throw 'ffprobe is not available on PATH.'
}
$audioDecoder = Join-Path $repoRoot 'build\runtime\Release\sandec_audio_cache.exe'
if (-not $FirstFrameOnly -and -not (Test-Path -LiteralPath $audioDecoder -PathType Leaf)) {
    throw "Missing SAN audio decoder: $audioDecoder. Build the sandec_audio_cache target first."
}
if (-not (Test-Path -LiteralPath $inputAbsolute -PathType Container)) {
    throw "Missing SAN input directory: $inputAbsolute"
}

$movies = @(Get-ChildItem -LiteralPath $inputAbsolute -Filter '*.SAN' -File |
    Sort-Object Name)
if ($MovieName.Count -gt 0) {
    $wanted = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
    foreach ($name in $MovieName) {
        foreach ($part in ($name -split ',')) {
            $candidate = [System.IO.Path]::GetFileName($part.Trim())
            if ([string]::IsNullOrWhiteSpace($candidate)) {
                continue
            }
            if (-not $candidate.EndsWith('.SAN', [StringComparison]::OrdinalIgnoreCase)) {
                $candidate = "$candidate.SAN"
            }
            [void]$wanted.Add($candidate)
        }
    }
    $movies = @($movies | Where-Object { $wanted.Contains($_.Name) })
}
if ($movies.Count -eq 0) {
    throw "No matching root-level .SAN files found in $inputAbsolute"
}

New-Item -ItemType Directory -Path $cacheAbsolute -Force | Out-Null
$manifest = New-Object System.Collections.Generic.List[string]
$manifest.Add("name`tbytes`twidth`theight`tfps`tframes`tduration`tcache`tstatus")

foreach ($movie in $movies) {
    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($movie.Name)
    $movieCache = Join-Path $cacheAbsolute $baseName
    New-Item -ItemType Directory -Path $movieCache -Force | Out-Null

    $probeJson = & ffprobe -v error -select_streams v:0 `
        -show_entries stream=width,height,avg_frame_rate,nb_frames,duration `
        -of json $movie.FullName
    if ($LASTEXITCODE -ne 0) {
        throw "ffprobe failed for $($movie.Name)"
    }
    $probe = $probeJson | ConvertFrom-Json
    $stream = $probe.streams[0]
    $width = [int]$stream.width
    $height = [int]$stream.height
    $fps = [string]$stream.avg_frame_rate
    $frames = [string]$stream.nb_frames
    $duration = [string]$stream.duration

    $framesPath = Join-Path $movieCache 'frames.rgba'
    $audioPath = Join-Path $movieCache 'audio.pcm'
    $metadataPath = Join-Path $movieCache 'metadata.tsv'
    $frameArgs = @('-hide_banner', '-loglevel', 'error', '-y', '-i',
        $movie.FullName)
    if ($FirstFrameOnly) {
        $frameArgs += @('-frames:v', '1')
    }
    $frameArgs += @('-pix_fmt', 'rgba', '-f', 'rawvideo', $framesPath)
    if (-not $AudioOnly) {
        & ffmpeg @frameArgs
        if ($LASTEXITCODE -ne 0) {
            throw "frame decode failed for $($movie.Name)"
        }
    }
    elseif (-not (Test-Path -LiteralPath $framesPath -PathType Leaf)) {
        throw "missing existing video cache for $($movie.Name): $framesPath"
    }

    $audioStatus = 'audio_none'
    if (-not $FirstFrameOnly) {
        if (Test-Path -LiteralPath $audioPath) {
            Remove-Item -LiteralPath $audioPath -Force
        }
        & $audioDecoder $movie.FullName $audioPath
        if ($LASTEXITCODE -ne 0) {
            throw "audio decode failed for $($movie.Name)"
        }
        if ((Test-Path -LiteralPath $audioPath -PathType Leaf) -and
            (Get-Item -LiteralPath $audioPath).Length -gt 0) {
            $audioStatus = 'audio_ok'
        }
    }

    @(
        "name`t$($movie.Name)",
        "bytes`t$($movie.Length)",
        "width`t$width",
        "height`t$height",
        "fps`t$fps",
        "frames`t$frames",
        "duration`t$duration",
        "frames_rgba`t$framesPath",
        "audio_pcm`t$audioPath",
        "audio_rate`t22050",
        "audio_status`t$audioStatus"
    ) | Set-Content -LiteralPath $metadataPath -Encoding UTF8

    $manifest.Add(
        "$($movie.Name)`t$($movie.Length)`t$width`t$height`t$fps`t$frames`t$duration`t$movieCache`t$audioStatus")
}

$manifestPath = Join-Path $cacheAbsolute 'manifest.tsv'
$manifest | Set-Content -LiteralPath $manifestPath -Encoding UTF8
Write-Host "Prepared SAN cache for $($movies.Count) movie(s)."
Write-Host "Manifest: $manifestPath"
