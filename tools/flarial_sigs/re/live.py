import ctypes, ctypes.wintypes as w, struct, sys
k=ctypes.WinDLL("kernel32",use_last_error=True)
class ME(ctypes.Structure):
    _fields_=[("dwSize",w.DWORD),("id",w.DWORD),("pid",w.DWORD),("g",w.DWORD),("p",w.DWORD),("base",ctypes.c_void_p),("size",w.DWORD),("h",ctypes.c_void_p),("name",ctypes.c_char*256),("path",ctypes.c_char*260)]
k.CreateToolhelp32Snapshot.restype=ctypes.c_void_p
k.Module32First.argtypes=[ctypes.c_void_p,ctypes.POINTER(ME)]; k.Module32Next.argtypes=[ctypes.c_void_p,ctypes.POINTER(ME)]
k.OpenProcess.restype=ctypes.c_void_p; k.OpenProcess.argtypes=[ctypes.c_uint32,ctypes.c_bool,ctypes.c_uint32]
k.ReadProcessMemory.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t,ctypes.POINTER(ctypes.c_size_t)]
def modules(pid):
    s=k.CreateToolhelp32Snapshot(0x18,pid); me=ME(); me.dwSize=ctypes.sizeof(ME); out={}
    ok=k.Module32First(s,ctypes.byref(me))
    while ok:
        out[me.name.decode().lower()]=(me.base,me.size); ok=k.Module32Next(s,ctypes.byref(me))
    return out
class Proc:
    def __init__(s,pid): s.h=k.OpenProcess(0x410,False,pid)
    def read(s,a,n):
        buf=ctypes.create_string_buffer(n); got=ctypes.c_size_t()
        if not a or not k.ReadProcessMemory(s.h,ctypes.c_void_p(a),buf,n,ctypes.byref(got)): return None
        return buf.raw[:got.value]
    def q(s,a):
        r=s.read(a,8); return struct.unpack('<Q',r)[0] if r and len(r)==8 else None
    def stdstr(s,a):
        r=s.read(a,32)
        if not r: return None
        size,cap=struct.unpack_from('<QQ',r,16)
        if size==0 or size>200 or cap<size or cap>0x10000: return None
        data=r[:16] if cap<16 else s.read(struct.unpack_from('<Q',r,0)[0],size)
        if not data: return None
        t=data[:size]
        return t.decode('latin1') if all(32<=c<127 for c in t) else None
