import ctypes, ctypes.wintypes as w, struct, json, sys
k = ctypes.WinDLL("kernel32", use_last_error=True)
pid = int(sys.argv[1]); out = sys.argv[2]
class ME(ctypes.Structure):
    _fields_ = [("dwSize", w.DWORD), ("th32ModuleID", w.DWORD), ("th32ProcessID", w.DWORD), ("GlblcntUsage", w.DWORD),
                ("ProccntUsage", w.DWORD), ("modBaseAddr", ctypes.c_void_p), ("modBaseSize", w.DWORD), ("hModule", ctypes.c_void_p),
                ("szModule", ctypes.c_char * 256), ("szExePath", ctypes.c_char * 260)]
k.CreateToolhelp32Snapshot.restype = ctypes.c_void_p
snap = k.CreateToolhelp32Snapshot(0x18, pid)
me = ME(); me.dwSize = ctypes.sizeof(ME)
k.Module32First.argtypes = [ctypes.c_void_p, ctypes.POINTER(ME)]
assert k.Module32First(snap, ctypes.byref(me)), ctypes.get_last_error()
base, size = me.modBaseAddr, me.modBaseSize
print(me.szModule, hex(base), hex(size))
k.OpenProcess.restype = ctypes.c_void_p
k.OpenProcess.argtypes = [ctypes.c_uint32, ctypes.c_bool, ctypes.c_uint32]
k.ReadProcessMemory.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
h = k.OpenProcess(0x410, False, pid)
img = bytearray(size)
step = 0x10000
for at in range(0, size, step):
    n = min(step, size - at)
    buf = ctypes.create_string_buffer(n); got = ctypes.c_size_t()
    if k.ReadProcessMemory(h, base + at, buf, n, ctypes.byref(got)):
        img[at:at+got.value] = buf.raw[:got.value]
open(out, "wb").write(img)
pe = struct.unpack_from("<I", img, 0x3C)[0]
count = struct.unpack_from("<H", img, pe + 6)[0]; osz = struct.unpack_from("<H", img, pe + 20)[0]
secs = []
for i in range(count):
    at = pe + 24 + osz + i * 40
    name = img[at:at+8].rstrip(b"\0").decode()
    vsize, rva = struct.unpack_from("<II", img, at + 8)
    flags = struct.unpack_from("<I", img, at + 36)[0]
    secs.append({"name": name, "rva": rva, "size": vsize, "exec": bool(flags & 0x20000000)})
json.dump({"base": base, "sections": secs}, open(out + ".json", "w"), indent=1)
print(secs)
