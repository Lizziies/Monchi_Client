rt.run("lib")
local out = {}
for _, r in ipairs({0x2078fbd12d0, 0x2079d4536f0, 0x207afa16c00, 0x20875b3a3b0}) do
    out[#out + 1] = string.format("== %x", r)
    for k = -0x40, 0x40, 8 do
        local q = rt.u64(r + k) or 0
        local note = ""
        if q > 0x10000 and q < 0x7fffffffffff then
            local v = rt.u64(q) or 0
            if v >= BASE and v < BASE + 0x14000000 then note = string.format(" -> vt %x id %x", v - BASE, rt.u32(q + 0x330) or 0) end
        end
        out[#out + 1] = string.format("  %4d %x%s", k, q, note)
    end
end
rt.out("act3.txt", table.concat(out, "\n"))
rt.log("act3 fin")
