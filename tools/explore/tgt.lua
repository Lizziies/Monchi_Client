rt.run("lib")
local hr = rt.u64(rt.u64(BASE + 0x11d61a70) + 0x1e8)
local out = {string.format("type %d hit %.2f %.2f %.2f", rt.i32(hr + 0x18), rt.f32(hr + 0x2c), rt.f32(hr + 0x30), rt.f32(hr + 0x34))}
for o = 0x38, 0x58, 8 do
    local p = rt.u64(hr + o) or 0
    out[#out + 1] = string.format("hr+%x = %x", o, p)
    if p > 0x10000 and p < 0x7fffffffffff then
        for k = 0, 0x40, 8 do
            local q = rt.u64(p + k) or 0
            local note = (q >= BASE and q < BASE + 0x14000000) and string.format(" vt %x", q - BASE) or ""
            out[#out + 1] = string.format("   +%x %x%s", k, q, note)
        end
    end
end
rt.out("tgt.txt", table.concat(out, "\n"))
rt.log("tgt fin")
