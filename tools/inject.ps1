param(
    [string]$Dll = (Join-Path $PSScriptRoot '..\build\Release\Monchi.dll'),
    # built with cmake -S dll -B C:\mfb -DMONCHI_FLARIAL=ON (a short path: Flarial's object paths are deep)
    [string]$Core = 'C:\mfb\Release\MonchiFlarial.dll',
    [int]$WaitSeconds = 120,
    [int]$ProcessId = 0,
    [switch]$Launch,
    [switch]$IsolatedData,
    [switch]$Dev
)

if ($Dev) { $Dll = Join-Path $PSScriptRoot '..\build-dev\Release\Monchi.dll' }

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;

public static class Inj {
    [DllImport("kernel32.dll", SetLastError = true)] static extern IntPtr OpenProcess(uint access, bool inherit, int pid);
    [DllImport("kernel32.dll", SetLastError = true)] static extern IntPtr VirtualAllocEx(IntPtr p, IntPtr addr, UIntPtr size, uint type, uint prot);
    [DllImport("kernel32.dll", SetLastError = true)] static extern bool WriteProcessMemory(IntPtr p, IntPtr addr, byte[] buf, UIntPtr size, out UIntPtr written);
    [DllImport("kernel32.dll", SetLastError = true)] static extern IntPtr CreateRemoteThread(IntPtr p, IntPtr attr, UIntPtr stack, IntPtr start, IntPtr arg, uint flags, out uint id);
    [DllImport("kernel32.dll")] static extern uint WaitForSingleObject(IntPtr h, uint ms);
    [DllImport("kernel32.dll")] static extern bool GetExitCodeThread(IntPtr h, out uint code);
    [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);
    [DllImport("kernel32.dll", CharSet = CharSet.Ansi)] static extern IntPtr GetProcAddress(IntPtr m, string name);
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)] static extern IntPtr GetModuleHandle(string name);

    public static string Load(int pid, string dll) {
        const uint access = 0x0002 | 0x0400 | 0x0008 | 0x0020 | 0x0010;
        IntPtr proc = OpenProcess(access, false, pid);
        if (proc == IntPtr.Zero) return "OpenProcess failed " + Marshal.GetLastWin32Error();
        byte[] bytes = Encoding.Unicode.GetBytes(dll + "\0");
        IntPtr remote = VirtualAllocEx(proc, IntPtr.Zero, (UIntPtr)bytes.Length, 0x3000, 0x04);
        UIntPtr written;
        if (remote == IntPtr.Zero || !WriteProcessMemory(proc, remote, bytes, (UIntPtr)bytes.Length, out written)) {
            CloseHandle(proc);
            return "write failed " + Marshal.GetLastWin32Error();
        }
        IntPtr kernel = GetModuleHandle("kernel32.dll");
        long load = (long)GetProcAddress(kernel, "LoadLibraryW");
        long last = (long)GetProcAddress(kernel, "GetLastError");
        // sub rsp,28; call LoadLibraryW; test rax,rax; jnz done; call GetLastError; done: add rsp,28; ret
        byte[] stub = new byte[39];
        int i = 0;
        stub[i++] = 0x48; stub[i++] = 0x83; stub[i++] = 0xEC; stub[i++] = 0x28;
        stub[i++] = 0x48; stub[i++] = 0xB8; BitConverter.GetBytes(load).CopyTo(stub, i); i += 8;
        stub[i++] = 0xFF; stub[i++] = 0xD0;
        stub[i++] = 0x48; stub[i++] = 0x85; stub[i++] = 0xC0;
        stub[i++] = 0x75; stub[i++] = 0x0C;
        stub[i++] = 0x48; stub[i++] = 0xB8; BitConverter.GetBytes(last).CopyTo(stub, i); i += 8;
        stub[i++] = 0xFF; stub[i++] = 0xD0;
        stub[i++] = 0x48; stub[i++] = 0x83; stub[i++] = 0xC4; stub[i++] = 0x28;
        stub[i++] = 0xC3;
        IntPtr code_ = VirtualAllocEx(proc, IntPtr.Zero, (UIntPtr)stub.Length, 0x3000, 0x40);
        if (code_ == IntPtr.Zero || !WriteProcessMemory(proc, code_, stub, (UIntPtr)stub.Length, out written)) {
            CloseHandle(proc);
            return "stub failed " + Marshal.GetLastWin32Error();
        }
        uint tid;
        IntPtr th = CreateRemoteThread(proc, IntPtr.Zero, UIntPtr.Zero, code_, remote, 0, out tid);
        if (th == IntPtr.Zero) { CloseHandle(proc); return "CreateRemoteThread failed " + Marshal.GetLastWin32Error(); }
        WaitForSingleObject(th, 20000);
        uint code;
        GetExitCodeThread(th, out code);
        CloseHandle(th);
        CloseHandle(proc);
        return code > 0x10000 ? "ok" : "LoadLibrary failed, error " + code;
    }
}
'@

$dll = (Resolve-Path $Dll).Path
$devDir = Join-Path (Resolve-Path (Join-Path $PSScriptRoot '..')).Path 'dev-data'
$bin = Join-Path $devDir 'bin'
New-Item -ItemType Directory -Force $bin, (Join-Path $devDir 'data') | Out-Null
$target = Join-Path $bin ("Monchi-{0:yyMMdd-HHmmss}.dll" -f (Get-Date))
Copy-Item $dll $target -Force
# the Flarial core is loaded by the client from its own folder when it is there
if (Test-Path $Core) { Copy-Item $Core (Join-Path $bin 'MonchiFlarial.dll') -Force -ErrorAction SilentlyContinue }
$sigs = Join-Path $PSScriptRoot '..\sigs'
if (Test-Path $sigs) {
    New-Item -ItemType Directory -Force (Join-Path $bin 'sigs') | Out-Null
    Copy-Item (Join-Path $sigs '*.json') (Join-Path $bin 'sigs') -Force
}
$clientData = if ($IsolatedData) { Join-Path $devDir 'data' } else { Join-Path $env:LOCALAPPDATA 'Monchi' }
New-Item -ItemType Directory -Force $clientData | Out-Null
[IO.File]::WriteAllText((Join-Path $bin 'Monchi.root'), $clientData)
$marker = Join-Path $bin 'Monchi.explore'
if ($Dev) { [IO.File]::WriteAllText($marker, (Resolve-Path (Join-Path $PSScriptRoot 'explore')).Path) }
elseif (Test-Path $marker) { Remove-Item $marker }
Get-ChildItem $bin -Filter 'Monchi-*.dll' | Where-Object FullName -ne $target | ForEach-Object { Remove-Item $_.FullName -ErrorAction SilentlyContinue }
icacls $target /grant '*S-1-15-2-1:(RX)' | Out-Null

if ($ProcessId) {
    Write-Host "inject pid ${ProcessId}: $([Inj]::Load($ProcessId, $target))"
    exit 0
}

if ($Launch -and -not (Get-Process Minecraft.Windows -ErrorAction SilentlyContinue)) {
    Start-Process 'explorer.exe' 'shell:AppsFolder\Microsoft.MinecraftUWP_8wekyb3d8bbwe!Game'
}

$deadline = (Get-Date).AddSeconds($WaitSeconds)
$retryAt = (Get-Date).AddSeconds(30)
$mc = $null
while ((Get-Date) -lt $deadline) {
    $mc = @(Get-Process Minecraft.Windows -ErrorAction SilentlyContinue)[0]
    if (-not $mc -and $Launch -and (Get-Date) -gt $retryAt) {
        Start-Process 'explorer.exe' 'shell:AppsFolder\Microsoft.MinecraftUWP_8wekyb3d8bbwe!Game'
        $retryAt = (Get-Date).AddSeconds(30)
    }
    if ($mc) {
        $mods = $mc.Modules | ForEach-Object { $_.ModuleName }
        if ($mc.MainWindowHandle -ne 0 -and ($mods -contains 'd3d12.dll' -or $mods -contains 'd3d11.dll')) { break }
    }
    Start-Sleep -Milliseconds 500
}
if (-not $mc) { Write-Host 'Minecraft did not start'; exit 1 }
if ($mc.Modules | Where-Object { $_.ModuleName -like 'Monchi*.dll' }) { Write-Host 'already injected'; exit 0 }

Start-Sleep -Seconds 2
$r = [Inj]::Load($mc.Id, $target)
Write-Host "inject pid $($mc.Id): $r"
if ($r -ne 'ok') { exit 1 }
