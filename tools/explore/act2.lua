rt.run("lib")
local player = rt.u64(BASE + 0x11d61a70)
local er = rt.u64(rt.u64(player + 0x1e8) + 0x38)
local target = 0x207bee7c480
local out = {}
-- map every pool payload page range
local pages = {}
local b, e = rt.u64(er + 0x98), rt.u64(er + 0xa0)
for n = b, e - 32, 32 do
    local key, pool = rt.u32(n + 8), rt.u64(n + 0x10)
    local vb, ve = rt.u64(pool + 0x50), rt.u64(pool + 0x58)
    if vb and ve and ve > vb and ve - vb <= 0x400 then
        for pg = vb, ve - 8, 8 do pages[#pages + 1] = {rt.u64(pg), key, pool} end
    end
end
for _, h in ipairs(rt.heap(target, 40)) do
    local where = ""
    for _, p in ipairs(pages) do if p[1] and h >= p[1] and h < p[1] + 0x40000 then where = string.format(" in pool %08x (%x) page off %x", p[2], p[3], h - p[1]) end end
    out[#out + 1] = string.format("ref at %x%s", h, where)
end
for _, h in ipairs(rt.heap(player, 60)) do
    for _, p in ipairs(pages) do if p[1] and h >= p[1] and h < p[1] + 0x40000 then out[#out + 1] = string.format("player ref at %x in pool %08x (%x) off %x", h, p[2], p[3], h - p[1]) end end
end
rt.out("act2.txt", table.concat(out, "\n"))
rt.log("act2 fin")
