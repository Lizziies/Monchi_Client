rt.run("lib")
local out = {}
for _, a in ipairs({0x210c5609674, 0x2117fb92080, 0x211df075330}) do
    out[#out + 1] = string.format("== %x", a)
    for k = -0x60, 0x40, 4 do
        local v = rt.f32(a + k) or 0
        local q = (k % 8 == (a % 8 == 0 and 0 or 4)) and rt.u64(a + k) or nil
        local note = ""
        if q and q >= BASE and q < BASE + 0x14000000 then note = string.format("  vt %x", q - BASE) end
        out[#out + 1] = string.format("  %5d  %10.3f%s", k, v, note)
    end
end
rt.out("hp.txt", table.concat(out, "\n"))
rt.log("hp fin")
