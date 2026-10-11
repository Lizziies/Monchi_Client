$ErrorActionPreference = 'Stop'
$source = Join-Path $PSScriptRoot '../../launcher/res/version_install.ps1'
$tokens = $null
$errors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile($source, [ref]$tokens, [ref]$errors)
if ($errors.Count) { throw 'Installer syntax failed' }
foreach ($name in 'Get-SettingsHash', 'Save-GameSettings', 'Restore-GameSettings') {
    $function = $ast.Find({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name }, $true)
    Invoke-Expression $function.Extent.Text
}
$temp = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$root = Join-Path $temp ('MonchiSettingsTest-' + [Guid]::NewGuid())
try {
    $game = Join-Path $root 'game/User/games/com.mojang/minecraftpe'
    New-Item -ItemType Directory -Path $game -Force | Out-Null
    $options = Join-Path $game 'options.txt'
    [IO.File]::WriteAllBytes($options, [Text.Encoding]::UTF8.GetBytes("gfx_field_of_view:85`r`nctrl_sensitivity:0.4`r`n"))
    $before = (Get-SettingsHash $options)
    $records = @(Save-GameSettings @((Join-Path $root 'game')) (Join-Path $root 'backup'))
    if ($records.Count -ne 1) { throw 'Expected one settings file' }
    [IO.File]::WriteAllText($options, 'changed by installation')
    Restore-GameSettings $records
    if ((Get-SettingsHash $options) -ne $before) { throw 'Modified settings not restored' }
    Remove-Item -LiteralPath $options
    Restore-GameSettings $records
    if ((Get-SettingsHash $options) -ne $before) { throw 'Missing settings not restored' }
    [IO.File]::WriteAllText($records[0].copy, 'broken backup')
    Remove-Item -LiteralPath $options
    $rejected = $false
    try { Restore-GameSettings $records } catch { $rejected = $true }
    if (!$rejected) { throw 'Corrupt backup accepted' }
    if (Test-Path -LiteralPath $options) { throw 'Corrupt settings copied into game data' }
} finally {
    $resolved = [IO.Path]::GetFullPath($root)
    if (!$resolved.StartsWith($temp, [StringComparison]::OrdinalIgnoreCase) -or !(Split-Path $resolved -Leaf).StartsWith('MonchiSettingsTest-')) { throw 'Unexpected test path' }
    if (Test-Path -LiteralPath $resolved) { Remove-Item -LiteralPath $resolved -Recurse -Force }
}
