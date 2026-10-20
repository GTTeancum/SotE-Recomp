[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$PackDirectory
)
$ErrorActionPreference = 'Stop'
$packRoot = (Resolve-Path -LiteralPath $PackDirectory).Path
$manifest = Join-Path $packRoot 'rt64.json'
$database = Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json
$verified = Get-Content -LiteralPath "$PSScriptRoot\..\config\v1_texture_bindings.json" -Raw | ConvertFrom-Json
if ($verified.version -ne 1) { throw 'Unsupported verified binding catalog.' }
$additions = @()
foreach ($binding in $verified.bindings) {
    if ($binding.rt64 -notmatch '^[0-9a-f]{16}$') { throw 'Invalid verified texture hash.' }
    $image = [IO.Path]::GetFullPath((Join-Path $packRoot ($binding.path + '.png')))
    if (-not $image.StartsWith($packRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Verified texture path escapes pack.'
    }
    if (-not (Test-Path -LiteralPath $image -PathType Leaf)) {
        throw "Verified replacement image missing: $($binding.path)"
    }
    $existing = @($database.textures | Where-Object { $_.hashes.rt64 -eq $binding.rt64 })
    if (@($existing | Where-Object { $_.path -ne $binding.path }).Count) {
        throw "Texture hash already belongs to another image: $($binding.rt64)"
    }
    if (-not $existing.Count) {
        $additions += [pscustomobject]@{ hashes = @{ rt64 = $binding.rt64 }; path = $binding.path }
    }
}
if ($additions.Count) {
    $database.textures = @($database.textures) + $additions
    [IO.File]::WriteAllText($manifest, ($database | ConvertTo-Json -Depth 20), [Text.UTF8Encoding]::new($false))
}
Write-Host "Added $($additions.Count) verified V1 texture binding(s). Existing bindings and artwork preserved."
