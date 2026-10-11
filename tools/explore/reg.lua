rt.run("lib")
local hr = rt.u64(rt.u64(BASE + 0x11d61a70) + 0x1e8)
local id = rt.u32(hr + 0x48)
local ctx = rt.u64(hr + 0x38)
local reg = rt.u64(ctx + 0x10)
local out = {string.format("id %x ctx %x reg %x", id, ctx, reg)}
for k = 0, 0x100, 8 do
    local q = rt.u64(reg + k) or 0
    local note = (q >= BASE and q < BASE + 0x14000000) and string.format(" vt %x", q - BASE) or ""
    out[#out + 1] = string.format("  reg+%x %x%s", k, q, note)
end
-- where else does this entity id occur near pointers to objects with image vtables (actor)?
local hits = rt.heap(id, 60)
for _, h in ipairs(hits) do
    local line = {}
    for back = 8, 0x40, 8 do local v = rt.u64(h - back); if v and v >= BASE and v < BASE + 0x14000000 then line[#line + 1] = string.format("-%x:%x", back, v - BASE) end end
    out[#out + 1] = string.format("id at %x  %s", h, table.concat(line, " "))
end
rt.out("reg.txt", table.concat(out, "\n"))
rt.log("reg fin")
