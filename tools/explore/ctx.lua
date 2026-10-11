rt.run("lib")
local p = rt.u64(BASE + 0x11d61a70)
local hr = rt.u64(p + 0x1e8)
local out = {string.format("player %x vt rva %x  target id %x", p, rt.u64(p) - BASE, rt.u32(hr + 0x48))}
for k = 8, 0x30, 8 do out[#out + 1] = string.format("  p+%x = %x", k, rt.u64(p + k) or 0) end
local id = rt.u32(hr + 0x48)
-- actors holding the target id at +0x10 and the same registry at +0x8
for _, h in ipairs(rt.heap(id, 200)) do
    if rt.u64(h - 8) == rt.u64(p + 8) then
        local a = h - 0x10
        local v = rt.u64(a) or 0
        out[#out + 1] = string.format("candidate actor %x vt %s", a, (v >= BASE and v < BASE + 0x14000000) and string.format("rva %x", v - BASE) or string.format("%x", v))
    end
end
rt.out("ctx.txt", table.concat(out, "\n"))
rt.log("ctx fin")
