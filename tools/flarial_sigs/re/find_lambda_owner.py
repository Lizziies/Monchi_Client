import sys
import lib

# A lambda's RTTI name says which member function created it; the std::function wrapper's target_type() returns
# the type descriptor, so typeid ref -> target_type fn -> wrapper vtable -> lea in the owning function.
def owner(name):
    p = lib.img.find(name.encode() + b"\0")
    if p < 0:
        return []
    out = []
    for ref in lib.xrefs(p - 0x10, 20):
        start = ref
        while lib.img[start - 1] != 0xCC:
            start -= 1
        for slot in lib.xrefs_abs(start, ".rdata", 10):
            vt = lib.vt_start(slot)
            for site in lib.lea_refs(vt):
                f = lib.func_start(site)
                if f:
                    out.append((f[0], f[1] - f[0]))
    return sorted(set(out))


if __name__ == "__main__":
    for n in sys.argv[1:]:
        print(n[:90], [(hex(a), hex(s)) for a, s in owner(n)])
