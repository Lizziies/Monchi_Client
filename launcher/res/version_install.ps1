param([Parameter(Mandatory=$true)][string]$Request)
function Get-SettingsHash($Path) {
    $stream = [IO.File]::OpenRead($Path)
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '') }
    finally { $sha.Dispose(); $stream.Dispose() }
}

function Save-GameSettings($Roots, $Backup) {
    $folder = Join-Path $Backup 'settings'
    New-Item -ItemType Directory -Path $folder -Force | Out-Null
    $index = 0
    foreach ($root in $Roots) {
        if (!(Test-Path -LiteralPath $root)) { continue }
        foreach ($file in Get-ChildItem -LiteralPath $root -Filter options.txt -Recurse -File -ErrorAction Stop) {
            $copy = Join-Path $folder "$index.txt"
            Copy-Item -LiteralPath $file.FullName -Destination $copy -ErrorAction Stop
            [PSCustomObject]@{ path = $file.FullName; copy = $copy; hash = (Get-SettingsHash $copy) }
            $index++
        }
    }
}

function Restore-GameSettings($Settings) {
    foreach ($entry in $Settings) {
        if ((Get-SettingsHash $entry.copy) -ne $entry.hash) { throw "Minecraft settings backup is damaged" }
        $same = (Test-Path -LiteralPath $entry.path) -and ((Get-SettingsHash $entry.path) -eq $entry.hash)
        if (!$same) {
            New-Item -ItemType Directory -Path (Split-Path $entry.path) -Force | Out-Null
            Copy-Item -LiteralPath $entry.copy -Destination $entry.path -Force -ErrorAction Stop
        }
        if ((Get-SettingsHash $entry.path) -ne $entry.hash) { throw 'Minecraft settings could not be restored' }
    }
}

$ErrorActionPreference = 'Stop'
$r = Get-Content -LiteralPath $Request -Raw -Encoding UTF8 | ConvertFrom-Json
try {
    if (Get-Process Minecraft*,gamelaunchhelper -ErrorAction SilentlyContinue) { throw 'Close Minecraft before switching versions' }
    if (!(Get-AppxPackage Microsoft.GamingServices)) { throw 'Install Gaming Services from the Microsoft Store first' }
    $family = if ($r.preview) { 'Microsoft.MinecraftWindowsBeta' } else { 'Microsoft.MinecraftUWP' }
    $installed = Get-AppxPackage -Name $family | Select-Object -First 1
    if (!$installed -or $installed.IsDevelopmentMode) { throw 'Install Minecraft with your own Microsoft account first' }
    if ($r.validateOnly) { exit 0 }
    if (!(Test-Path -LiteralPath $r.package -PathType Leaf)) { throw 'The downloaded package is missing' }
    $backup = Join-Path $r.backup ([DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-ffff'))
    New-Item -ItemType Directory -Path $backup -Force | Out-Null
    $roots = @(
        (Join-Path $env:APPDATA 'Minecraft Bedrock'),
        (Join-Path $env:LOCALAPPDATA "Packages\$($installed.PackageFamilyName)\LocalState\games\com.mojang")
    )
    for ($i = 0; $i -lt $roots.Count; $i++) {
        if (Test-Path -LiteralPath $roots[$i]) {
            Copy-Item -LiteralPath $roots[$i] -Destination (Join-Path $backup "data-$i") -Recurse -ErrorAction Stop
        }
    }
    $settings = @(Save-GameSettings $roots $backup)
    $settings | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $backup "settings.json") -Encoding UTF8
    $roots | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $backup 'sources.json') -Encoding UTF8
    if (Get-Process Minecraft*,gamelaunchhelper -ErrorAction SilentlyContinue) { throw 'Close Minecraft before switching versions' }
    $deploymentStarted = $true
    Add-AppxPackage -Path $r.package -ForceUpdateFromAnyVersion -ErrorAction Stop
    Restore-GameSettings $settings
    $actual = (Get-AppxPackage -Name $family | Select-Object -First 1).Version
    $p = $r.version.Split('.')
    $expected = [Version]::new([int]$p[0], [int]$p[1], ([int]$p[2] * 100 + [int]$p[3]), 0)
    if ($actual -ne $expected) { throw 'Installed Minecraft version does not match the selected version' }
} catch {
    $reason = $_.Exception.Message
    if ($deploymentStarted -and !(Get-Process Minecraft*,gamelaunchhelper -ErrorAction SilentlyContinue)) {
        try { Restore-GameSettings $settings } catch { $reason += "; " + $_.Exception.Message }
    }
    [IO.File]::WriteAllText($r.error, $reason)
    exit 1
}
