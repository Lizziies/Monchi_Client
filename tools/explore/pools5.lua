rt.run("lib")
local player = rt.u64(BASE + 0x11d61a70)
local hr = rt.u64(player + 0x1e8)
local er = rt.u64(hr + 0x38)
local b, e = rt.u64(er + 0x98), rt.u64(er + 0xa0)
local out = {}
for n = b, e - 32, 32 do
    local key, pool = rt.u32(n + 8), rt.u64(n + 0x10)
    local vb, ve = rt.u64(pool + 0x50), rt.u64(pool + 0x58)
    local pb, pe = rt.u64(pool + 0x20), rt.u64(pool + 0x28)
    if vb and ve and ve > vb and ve - vb <= 0x100 and pb and pe and pe > pb then
        local count = (pe - pb) // 4
        local page = rt.u64(vb)
        local raw = page and rt.raw(page, math.min(0x2000, count * 0x40 + 8))
        if raw then
            local hits = {}
            for i = 1, #raw - 7, 8 do
                local q = string.unpack("<I8", raw, i)
                if q > 0x10000 and q < 0x7fffffffffff and rt.u64(q + 0x320) == er then hits[#hits + 1] = string.format("%x->%x id %x", i - 1, q, rt.u32(q + 0x330) or 0) end
                if #hits > 4 then break end
            end
            if #hits > 0 then out[#out + 1] = string.format("key %08x pool %x count %d: %s", key, pool, count, table.concat(hits, " ")) end
        end
    end
end
rt.out("pools5.txt", table.concat(out, "\n"))
rt.log("pools5 fin")
