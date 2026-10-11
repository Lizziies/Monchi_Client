param()
$ErrorActionPreference = 'Stop'
$taskRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class MonchiUnload {
    [DllImport("kernel32.dll")] static extern IntPtr GetCurrentProcess();
    [DllImport("dbghelp.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern bool SymInitializeW(IntPtr p, string path, bool invade);
    [DllImport("dbghelp.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern ulong SymLoadModuleExW(IntPtr p, IntPtr f, string image, string name, ulong b, uint size, IntPtr data, uint flags);
    [DllImport("dbghelp.dll", CharSet=CharSet.Ansi, SetLastError=true)] static extern bool SymFromName(IntPtr p, string name, IntPtr info);
    [DllImport("dbghelp.dll")] static extern bool SymCleanup(IntPtr p);
    [DllImport("kernel32.dll", SetLastError=true)] static extern IntPtr OpenProcess(uint rights, bool inherit, int pid);
    [DllImport("kernel32.dll", SetLastError=true)] static extern IntPtr CreateRemoteThread(IntPtr p, IntPtr attr, UIntPtr stack, IntPtr start, IntPtr arg, uint flags, out uint id);
    [DllImport("kernel32.dll")] static extern uint WaitForSingleObject(IntPtr h, uint ms);
    [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);
    public static void Request(int pid, string dll, string symbols, long remoteBase) {
        IntPtr local = GetCurrentProcess();
        if (!SymInitializeW(local, symbols, false)) throw new Exception("Symbol initialization failed");
        IntPtr info = Marshal.AllocHGlobal(1112);
        try {
            ulong loaded = SymLoadModuleExW(local, IntPtr.Zero, dll, "monchi", 0x10000000, 0, IntPtr.Zero, 0);
            if (loaded == 0) throw new Exception("Matching DLL symbols unavailable");
            Marshal.WriteInt32(info, 0, 88);
            Marshal.WriteInt32(info, 80, 1024);
            if (!SymFromName(local, "monchi!client::requestUnload", info)) throw new Exception("requestUnload symbol unavailable");
            long address = Marshal.ReadInt64(info, 56);
            long offset = address - (long)loaded;
            if (offset <= 0 || offset > new System.IO.FileInfo(dll).Length) throw new Exception("Invalid unload symbol");
            IntPtr process = OpenProcess(0x43a, false, pid);
            if (process == IntPtr.Zero) throw new Exception("OpenProcess failed");
            try {
                uint id;
                IntPtr thread = CreateRemoteThread(process, IntPtr.Zero, UIntPtr.Zero, new IntPtr(remoteBase + offset), IntPtr.Zero, 0, out id);
                if (thread == IntPtr.Zero) throw new Exception("Unload request failed");
                try { if (WaitForSingleObject(thread, 5000) != 0) throw new Exception("Unload request timeout"); }
                finally { CloseHandle(thread); }
            } finally { CloseHandle(process); }
        } finally { Marshal.FreeHGlobal(info); SymCleanup(local); }
    }
}
'@
$taskGame = Get-Process Minecraft.Windows | Select-Object -First 1
$taskModules = @($taskGame.Modules | Where-Object ModuleName -like 'Monchi*.dll')
foreach ($taskModule in $taskModules) {
    [MonchiUnload]::Request($taskGame.Id, $taskModule.FileName, (Join-Path $taskRoot 'build-dev\Release'), $taskModule.BaseAddress.ToInt64())
}
for ($taskAttempt = 0; $taskAttempt -lt 80; $taskAttempt++) {
    $taskGame.Refresh()
    if (-not @($taskGame.Modules | Where-Object ModuleName -like 'Monchi*.dll').Count) { Write-Host 'Monchi unloaded; no injection'; exit 0 }
    Start-Sleep -Milliseconds 250
}
throw 'Monchi did not unload'
