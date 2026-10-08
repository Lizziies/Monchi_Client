rt.run("lib")
local player = rt.u64(BASE + 0x11d61a70)
local hr = rt.u64(player + 0x1e8)
local er = rt.u64(hr + 0x38)
local b, e = rt.u64(er + 0x98), rt.u64(er + 0xa0)
local pat = string.pack("<I8", player)
local out = {}
for n = b, e - 32, 32 do
    local key, pool = rt.u32(n + 8), rt.u64(n + 0x10)
    for _, off in ipairs({0x50, 0x88}) do
        local vb, ve = rt.u64(pool + off), rt.u64(pool + off + 8)
        if vb and ve and ve > vb and ve - vb <= 0x400 then
            for pg = vb, ve - 8, 8 do
                local page = rt.u64(pg)
                local raw = page and page > 0x10000 and rt.raw(page, 0x10000)
                if raw then
                    local s = raw:find(pat, 1, true)
                    if s then
                        local pb, pe = rt.u64(pool + 0x20), rt.u64(pool + 0x28)
                        out[#out + 1] = string.format("key %08x pool %x vt %x payload@+%x page %d off %x  packed %d", key, pool, rt.u64(pool) - BASE, off, (pg - vb) // 8, s - 1, (pe - pb) // 4)
                    end
                end
            end
        end
    end
end
rt.out("pools3.txt", table.concat(out, "\n"))
rt.log("pools3 fin")
