"""Sampling profiler for the test hosts: suspends the frame thread, walks its stack with dbghelp and counts where the time
inside Monchi's frame goes. It starts the test host itself and only ever looks at that process.

usage: sampler.py <seconds> <Monchi.dll> [script] [symbol dir] [root function]
env:   SAMPLER_HOSTS  folder with testhost.exe and testhost12.exe
       SAMPLER_BENCH  folder the dll runs in (put a Monchi.root file there so the real settings stay untouched)
       SAMPLER_DX12   set to profile the D3D12 host (Monchi then draws through D3D11On12)

The dll has to be built with symbols (/Zi and /DEBUG), the pdb is found through the symbol dir."""
import ctypes, ctypes.wintypes as w, os, subprocess, sys, time, collections

k32 = ctypes.WinDLL('kernel32', use_last_error=True)
dbg = ctypes.WinDLL('dbghelp', use_last_error=True)

seconds = float(sys.argv[1]) if len(sys.argv) > 1 else 10
dll = sys.argv[2] if len(sys.argv) > 2 else 'Monchi.dll'
script = sys.argv[3] if len(sys.argv) > 3 else ''
symdir = sys.argv[4] if len(sys.argv) > 4 else os.path.dirname(os.path.abspath(dll))

env = dict(os.environ, MONCHI_BENCH='1', TESTHOST_MANUAL='1', TESTHOST_SIZE='1920x1080', TESTHOST_FPS='4000')
if script:
    env['TESTHOST_SCRIPT'] = script
hosts = os.path.abspath(os.environ.get('SAMPLER_HOSTS', '.'))
if os.environ.get('SAMPLER_BENCH'):
    os.chdir(os.environ['SAMPLER_BENCH'])
host = os.path.join(hosts, 'testhost12.exe' if os.environ.get('SAMPLER_DX12') else 'testhost.exe')
proc = subprocess.Popen([host, dll, str(int(seconds + 8))], env=env, stdout=subprocess.DEVNULL)
time.sleep(6.0)

PROCESS_ALL = 0x1F0FFF
k32.OpenProcess.restype = w.HANDLE
hproc = k32.OpenProcess(PROCESS_ALL, False, proc.pid)


class THREADENTRY32(ctypes.Structure):
    _fields_ = [('dwSize', w.DWORD), ('cntUsage', w.DWORD), ('th32ThreadID', w.DWORD), ('th32OwnerProcessID', w.DWORD),
                ('tpBasePri', w.LONG), ('tpDeltaPri', w.LONG), ('dwFlags', w.DWORD)]


k32.CreateToolhelp32Snapshot.restype = w.HANDLE
k32.OpenThread.restype = w.HANDLE
snap = k32.CreateToolhelp32Snapshot(4, 0)
te = THREADENTRY32()
te.dwSize = ctypes.sizeof(te)
threads = []
ok = k32.Thread32First(snap, ctypes.byref(te))
while ok:
    if te.th32OwnerProcessID == proc.pid:
        threads.append(te.th32ThreadID)
    ok = k32.Thread32Next(snap, ctypes.byref(te))
k32.CloseHandle(snap)

# the frame thread is the one that was created first
best, bestTime = None, None
for tid in threads:
    h = k32.OpenThread(0x1FFFFF, False, tid)
    c, e, kt, ut = w.FILETIME(), w.FILETIME(), w.FILETIME(), w.FILETIME()
    k32.GetThreadTimes(w.HANDLE(h), ctypes.byref(c), ctypes.byref(e), ctypes.byref(kt), ctypes.byref(ut))
    created = (c.dwHighDateTime << 32) | c.dwLowDateTime
    if bestTime is None or created < bestTime:
        if best:
            k32.CloseHandle(w.HANDLE(best))
        best, bestTime = h, created
    else:
        k32.CloseHandle(w.HANDLE(h))
hthread = w.HANDLE(best)

dbg.SymSetOptions(0x2 | 0x4 | 0x10)
dbg.SymInitializeW.argtypes = [w.HANDLE, w.LPCWSTR, w.BOOL]
if not dbg.SymInitializeW(w.HANDLE(hproc), symdir, True):
    print('SymInitialize failed', ctypes.get_last_error())


class ADDRESS64(ctypes.Structure):
    _fields_ = [('Offset', ctypes.c_uint64), ('Segment', w.WORD), ('Mode', ctypes.c_int)]


class KDHELP64(ctypes.Structure):
    _fields_ = [('Thread', ctypes.c_uint64), ('ThCallbackStack', w.DWORD), ('ThCallbackBStore', w.DWORD), ('NextCallback', w.DWORD),
                ('FramePointer', w.DWORD), ('KiCallUserMode', ctypes.c_uint64), ('KeUserCallbackDispatcher', ctypes.c_uint64),
                ('SystemRangeStart', ctypes.c_uint64), ('KiUserExceptionDispatcher', ctypes.c_uint64), ('StackBase', ctypes.c_uint64),
                ('StackLimit', ctypes.c_uint64), ('BuildVersion', w.DWORD), ('RetpolineStubFunctionTableSize', w.DWORD),
                ('RetpolineStubFunctionTable', ctypes.c_uint64), ('RetpolineStubOffset', w.DWORD), ('RetpolineStubSize', w.DWORD),
                ('Reserved0', ctypes.c_uint64 * 2)]


class STACKFRAME64(ctypes.Structure):
    _fields_ = [('AddrPC', ADDRESS64), ('AddrReturn', ADDRESS64), ('AddrFrame', ADDRESS64), ('AddrStack', ADDRESS64),
                ('AddrBStore', ADDRESS64), ('FuncTableEntry', ctypes.c_void_p), ('Params', ctypes.c_uint64 * 4), ('Far', w.BOOL),
                ('Virtual', w.BOOL), ('Reserved', ctypes.c_uint64 * 3), ('KdHelp', KDHELP64)]


class SYMBOL_INFO(ctypes.Structure):
    _fields_ = [('SizeOfStruct', w.ULONG), ('TypeIndex', w.ULONG), ('Reserved', ctypes.c_uint64 * 2), ('Index', w.ULONG), ('Size', w.ULONG),
                ('ModBase', ctypes.c_uint64), ('Flags', w.ULONG), ('Value', ctypes.c_uint64), ('Address', ctypes.c_uint64),
                ('Register', w.ULONG), ('Scope', w.ULONG), ('Tag', w.ULONG), ('NameLen', w.ULONG), ('MaxNameLen', w.ULONG),
                ('Name', ctypes.c_char * 512)]


dbg.StackWalk64.argtypes = [w.DWORD, w.HANDLE, w.HANDLE, ctypes.POINTER(STACKFRAME64), ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p,
                            ctypes.c_void_p, ctypes.c_void_p]
dbg.SymFromAddr.argtypes = [w.HANDLE, ctypes.c_uint64, ctypes.POINTER(ctypes.c_uint64), ctypes.POINTER(SYMBOL_INFO)]
table = ctypes.cast(dbg.SymFunctionTableAccess64, ctypes.c_void_p)
modbase = ctypes.cast(dbg.SymGetModuleBase64, ctypes.c_void_p)

raw = ctypes.create_string_buffer(1232 + 16)
base = ctypes.addressof(raw)
ctxAddr = (base + 15) & ~15
CONTEXT_FULL = 0x10000B


def u64(off):
    return ctypes.c_uint64.from_address(ctxAddr + off).value


stacks = []
end = time.time() + seconds
while time.time() < end:
    if k32.SuspendThread(hthread) == 0xFFFFFFFF:
        break
    ctypes.memset(ctxAddr, 0, 1232)
    ctypes.c_uint32.from_address(ctxAddr + 0x30).value = CONTEXT_FULL
    pcs = []
    if k32.GetThreadContext(hthread, ctypes.c_void_p(ctxAddr)):
        f = STACKFRAME64()
        f.AddrPC.Offset, f.AddrPC.Mode = u64(0xF8), 3
        f.AddrFrame.Offset, f.AddrFrame.Mode = u64(0xA0), 3
        f.AddrStack.Offset, f.AddrStack.Mode = u64(0x98), 3
        for _ in range(48):
            if not dbg.StackWalk64(0x8664, w.HANDLE(hproc), hthread, ctypes.byref(f), ctypes.c_void_p(ctxAddr), None, table, modbase, None):
                break
            if not f.AddrPC.Offset:
                break
            pcs.append(f.AddrPC.Offset)
    k32.ResumeThread(hthread)
    if pcs:
        stacks.append(pcs)
    time.sleep(0.0008)

names = {}


def name(pc):
    if pc in names:
        return names[pc]
    s = SYMBOL_INFO()
    s.SizeOfStruct = 88
    s.MaxNameLen = 500
    d = ctypes.c_uint64()
    n = s.Name.decode(errors='replace') if dbg.SymFromAddr(w.HANDLE(hproc), pc, ctypes.byref(d), ctypes.byref(s)) else hex(pc)
    names[pc] = n
    return n


root = sys.argv[5] if len(sys.argv) > 5 else 'ui::frame'
inside = 0
selfc, incl = collections.Counter(), collections.Counter()
for pcs in stacks:
    fn = [name(p) for p in pcs]
    if not any(root in x for x in fn):
        continue
    inside += 1
    cut = next(i for i, x in enumerate(fn) if root in x)
    fn = fn[:cut + 1]
    selfc[fn[0]] += 1
    for x in set(fn):
        incl[x] += 1


# which source line of the root function the time is under
class IMAGEHLP_LINE64(ctypes.Structure):
    _fields_ = [('SizeOfStruct', w.DWORD), ('Key', ctypes.c_void_p), ('LineNumber', w.DWORD), ('FileName', ctypes.c_char_p), ('Address', ctypes.c_uint64)]


dbg.SymGetLineFromAddr64.argtypes = [w.HANDLE, ctypes.c_uint64, ctypes.POINTER(w.DWORD), ctypes.POINTER(IMAGEHLP_LINE64)]
lines = collections.Counter()
for pcs in stacks:
    fn = [name(p) for p in pcs]
    for i, x in enumerate(fn):
        if root in x:
            line = IMAGEHLP_LINE64()
            line.SizeOfStruct = ctypes.sizeof(line)
            disp = w.DWORD()
            # a return address points behind the call, one byte back is still inside it
            pc = pcs[i] - (1 if i > 0 else 0)
            if dbg.SymGetLineFromAddr64(w.HANDLE(hproc), pc, ctypes.byref(disp), ctypes.byref(line)):
                lines[line.LineNumber] += 1
            break
print('--- by line of', root)
for ln, c in sorted(lines.items()):
    print('%5d  %5.1f%%' % (ln, 100.0 * c / max(1, inside)))
print('samples', len(stacks), 'inside', root, inside, '(%.1f%%)' % (100.0 * inside / max(1, len(stacks))))
print('--- inclusive (share of time inside %s)' % root)
for n, c in incl.most_common(45):
    print('%5.1f%%  %s' % (100.0 * c / max(1, inside), n[:110]))
print('--- self')
for n, c in selfc.most_common(30):
    print('%5.1f%%  %s' % (100.0 * c / max(1, inside), n[:110]))
proc.wait(timeout=60)
