rt.run("lib")
local er = rt.u64(rt.u64(BASE + 0x11d61a70) + 0x320)
local function poolOf(key)
    local b, e = rt.u64(er + 0x98), rt.u64(er + 0xa0)
    for n = b, e - 32, 32 do if rt.u32(n + 8) == key then return rt.u64(n + 0x10) end end
end
local function page(key) return rt.u64(rt.u64(poolOf(key) + 0x50)) end
local function ents(p) local t = {} for a = rt.u64(p + 0x20), rt.u64(p + 0x28) - 4, 4 do t[#t + 1] = string.format("%x", rt.u32(a)) end return table.concat(t, ",") end
local upd = poolOf(0x0e49cf3b)
local function dump(tag)
    local out = {tag}
    for _, k in ipairs({0xceb578f1, 0x2f0fc33f, 0xf5b4eb5c, 0x75df36b7, 0x113040a1}) do
        local p = poolOf(k)
        local pg = rt.u64(rt.u64(p + 0x50)) or 0
        out[#out + 1] = string.format("%08x ents %s : %s", k, ents(p), pg > 0x10000 and (rt.hex(pg, 0x50) or "?") or "-")
    end
    rt.log(table.concat(out, "\n   "))
end
dump("fl3 before")
local sp = rt.u64(rt.u64(upd + 8))
local saved = {}
for _, id in ipairs({2, 3, 4, 5, 7}) do
    saved[id] = rt.u32(sp + id * 4)
    rt.wf32(sp + id * 4, string.unpack("<f", string.pack("<I4", saved[id] ~ 0x00040000)))
end
rt.sleep(2500)
dump("fl3 during")
for id, v in pairs(saved) do rt.wf32(sp + id * 4, string.unpack("<f", string.pack("<I4", v))) end
