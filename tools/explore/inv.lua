rt.run("lib")
local er = rt.u64(rt.u64(BASE + 0x11d61a70) + 0x320)
local function poolOf(key)
    local b, e = rt.u64(er + 0x98), rt.u64(er + 0xa0)
    for n = b, e - 32, 32 do if rt.u32(n + 8) == key then return rt.u64(n + 0x10) end end
end
local function snap()
    local out = {}
    for _, k in ipairs({0xc67426f3, 0xe642016b}) do
        local p = poolOf(k)
        local t = {}
        for a = rt.u64(p + 0x20), rt.u64(p + 0x28) - 4, 4 do t[#t + 1] = string.format("%x", rt.u32(a)) end
        local pg = rt.u64(rt.u64(p + 0x50))
        out[#out + 1] = string.format("%08x %s %s", k, table.concat(t, ","), rt.hex(pg, 0x100) or "?")
    end
    return out
end
local a = snap()
rt.sleep(9000)
local b = snap()
for i = 1, #a do rt.log("inv A " .. a[i]) rt.log("inv B " .. b[i]) end
-- synched data item 0 of every entity (flags)
