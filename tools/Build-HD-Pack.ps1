[CmdletBinding()]
param(
    [string]$Source = 'texture_upgrade\rom',
    [string]$Output = 'textures\MyHDPack'
)
$ErrorActionPreference = 'Stop'
$sourceRoot = (Resolve-Path -LiteralPath $Source).Path.TrimEnd('\')
$outputRoot = if ([IO.Path]::IsPathRooted($Output)) { [IO.Path]::GetFullPath($Output) } else { [IO.Path]::GetFullPath((Join-Path (Get-Location) $Output)) }
if (Test-Path -LiteralPath $outputRoot) { throw 'Output already exists. Choose a new pack folder to protect existing artwork.' }
function Source-Path([string]$relative) {
    $full = [IO.Path]::GetFullPath((Join-Path $sourceRoot $relative))
    if (-not $full.StartsWith($sourceRoot + '\', [StringComparison]::OrdinalIgnoreCase)) { throw "Invalid source path: $relative" }
    return $full
}
$baseline = Get-Content -LiteralPath "$sourceRoot\source_baseline.json" -Raw | ConvertFrom-Json
if ($baseline.version -ne 1) { throw 'Unsupported source baseline.' }
$edited = @{}
foreach ($item in $baseline.images.PSObject.Properties) {
    if ([IO.Path]::GetExtension($item.Name) -ine '.png') { throw 'Stock baseline must reference PNG files.' }
    $path = Source-Path $item.Name
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { continue }
    $bytes = [IO.File]::ReadAllBytes($path)
    $hasher = [Security.Cryptography.SHA256]::Create()
    try { $digest = [BitConverter]::ToString($hasher.ComputeHash($bytes)).Replace('-','') } finally { $hasher.Dispose() }
    if ($digest -ieq $item.Value) { continue }
    if ($bytes.Length -lt 33 -or [BitConverter]::ToString($bytes,0,8) -ne '89-50-4E-47-0D-0A-1A-0A' -or
        [Text.Encoding]::ASCII.GetString($bytes,12,4) -ne 'IHDR') { throw "Invalid PNG: $($item.Name)" }
    foreach ($offset in @(16,20)) {
        $dimension = [uint64]$bytes[$offset]*16777216 + [uint64]$bytes[$offset+1]*65536 + [uint64]$bytes[$offset+2]*256 + $bytes[$offset+3]
        if ($dimension -lt 1 -or $dimension -gt 16384) { throw "Unsupported PNG size: $($item.Name)" }
    }
    $key = ($item.Name -replace '\\','/') -replace '\.png$',''
    $edited[$key] = $path
}
$database = Get-Content -LiteralPath "$sourceRoot\rt64.json" -Raw | ConvertFrom-Json
$bindings = @{}
foreach ($entry in $database.textures) {
    if ($edited.ContainsKey($entry.path)) { $bindings[$entry.hashes.rt64] = $entry }
}
$slots = @{}
if (Test-Path -LiteralPath "$sourceRoot\slot_catalog.json") {
    $catalog = Get-Content -LiteralPath "$sourceRoot\slot_catalog.json" -Raw | ConvertFrom-Json
    foreach ($entry in $catalog.slots) {
        if (-not $edited.ContainsKey($entry.path)) { continue }
        $slots[$entry.hash] = @{key=$entry.key;hash=$entry.hash}
        $bindings[$entry.hash] = @{hashes=@{rt64=$entry.hash};path=$entry.path}
    }
}
$bound = @{}
foreach ($entry in $bindings.Values) { $bound[$entry.path] = $true }
foreach ($key in $edited.Keys) { if (-not $bound.ContainsKey($key)) { throw "Edited PNG has no replacement binding: $key" } }
$database.textures = @($bindings.Values)
New-Item -ItemType Directory -Path $outputRoot | Out-Null
foreach ($key in $edited.Keys) {
    $destination = [IO.Path]::GetFullPath((Join-Path $outputRoot ($key + '.png')))
    if (-not $destination.StartsWith($outputRoot + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid destination path.' }
    New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
    Copy-Item -LiteralPath $edited[$key] -Destination $destination
}
$utf8 = [Text.UTF8Encoding]::new($false)
[IO.File]::WriteAllText("$outputRoot\rt64.json", ($database | ConvertTo-Json -Depth 20), $utf8)
[IO.File]::WriteAllText("$outputRoot\sote_slots.json", (@{version=1;slots=@($slots.Values)} | ConvertTo-Json -Depth 20), $utf8)
Write-Host "Built $($edited.Count) edited PNG(s), $($bindings.Count) bindings, $($slots.Count) source slots: $outputRoot"
Write-Host 'Restart the game to load the pack. Unedited textures keep their native rendering.'
