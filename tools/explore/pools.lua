rt.run("lib")
-- every component pool of the entity registry, named through the pool's RTTI
local er = rt.u64(rt.u64(BASE + 0x11d61a70) + 0x320)
local out = {}
local b, e = rt.u64(er + 0x98), rt.u64(er + 0xa0)
for n = b, e - 32, 32 do
    local key, pool = rt.u32(n + 8), rt.u64(n + 0x10)
    local vt = rt.u64(pool)
    local col = vt and rt.u64(vt - 8)
    local td = col and rt.u32(col + 12)
    local name = td and rt.cstr(BASE + td + 16, 200) or "?"
    local pb, pe = rt.u64(pool + 0x20), rt.u64(pool + 0x28)
    local count = (pb and pe) and (pe - pb) // 4 or -1
    out[#out + 1] = string.format("%08x %6d vt %x %s", key, count, vt and (vt - BASE) or 0, name)
end
rt.out("pools.txt", table.concat(out, "\n"))
rt.log("pools fin", #out)

