param([switch]$Dev, [switch]$NoBuild, [switch]$UnloadOnly)

# Builds the client, unloads the running copy with Ctrl+L and injects the new one into the same game process.
$root = Resolve-Path (Join-Path $PSScriptRoot '..')
Set-Location $root
$dir = if ($Dev) { 'build-dev' } else { 'build' }

if (-not $NoBuild -and -not $UnloadOnly) {
    $log = cmake --build $dir --config Release -- /m /nologo /v:m /clp:NoSummary 2>&1
    $errors = $log | Select-String 'error (C|LNK)\d+'
    if ($errors) { $errors | Select-Object -First 20 | ForEach-Object { $_.Line }; exit 1 }
}

$mc = Get-Process Minecraft.Windows -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $mc) { Write-Host 'Minecraft is not running'; exit 1 }

Add-Type -Namespace Reload -Name Native -MemberDefinition @'
[DllImport("user32.dll")] public static extern bool SetForegroundWindow(System.IntPtr h);
[DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, System.UIntPtr extra);
'@
$loaded = { ($mc.Refresh()); @($mc.Modules | Where-Object { $_.ModuleName -like 'Monchi*.dll' }).Count -gt 0 }
if (& $loaded) {
    [Reload.Native]::SetForegroundWindow($mc.MainWindowHandle) | Out-Null
    Start-Sleep -Milliseconds 300
    [Reload.Native]::keybd_event(0xA2, 0, 0, [UIntPtr]::Zero)
    [Reload.Native]::keybd_event(0x4C, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 80
    [Reload.Native]::keybd_event(0x4C, 0, 2, [UIntPtr]::Zero)
    [Reload.Native]::keybd_event(0xA2, 0, 2, [UIntPtr]::Zero)
    for ($i = 0; $i -lt 60 -and (& $loaded); $i++) { Start-Sleep -Milliseconds 250 }
    if (& $loaded) { Write-Host 'client did not unload'; exit 1 }
}

if ($UnloadOnly) { Write-Host 'Monchi unloaded; no injection'; exit 0 }

$inj = @('-NoProfile', '-File', (Join-Path $PSScriptRoot 'inject.ps1'), '-ProcessId', $mc.Id)
if ($Dev) { $inj += '-Dev' }
& powershell @inj
