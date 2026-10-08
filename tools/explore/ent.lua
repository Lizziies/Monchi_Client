rt.run("lib")
local hr = rt.u64(rt.u64(BASE + 0x11d61a70) + 0x1e8)
local er = rt.u64(hr + 0x38)
local out = {string.format("er %x name %s", er, rt.cstr(rt.u64(er + 0x10), 40) or "?")}
for k = 0x30, 0x140, 8 do
    local q = rt.u64(er + k) or 0
    local note = (q >= BASE and q < BASE + 0x14000000) and string.format(" vt %x", q - BASE) or ""
    out[#out + 1] = string.format("  er+%x %x%s", k, q, note)
end
rt.out("ent.txt", table.concat(out, "\n"))
rt.log("ent fin")
