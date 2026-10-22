<# Legacy entry point retained so old setup instructions cannot inflate the drive. #>
[CmdletBinding()]
param(
    [string]$InputDirectory = 'SotE_Recompiled\Sdata',
    [string]$CacheDirectory = '',
    [string[]]$MovieName = @(),
    [switch]$FirstFrameOnly,
    [switch]$AudioOnly
)
throw 'SAN preprocessing has been removed. Place original .SAN files in the playable Sdata folder; the game decodes them directly. No disk cache is supported.'
