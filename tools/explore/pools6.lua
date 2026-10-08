rt.run("lib")
local player = rt.u64(BASE + 0x11d61a70)
local er = rt.u64(rt.u64(player + 0x1e8) + 0x38)
local attrVt, hungerDef = BASE + 0xe778140, BASE + 0x11d1ecf8
local arrays = {}
for _, h in ipairs(rt.heap(hungerDef, 100)) do if rt.u64(h - 8) == attrVt then arrays[#arrays + 1] = h - 8 end end
local out = {"attr arrays: " .. #arrays}
local b, e = rt.u64(er + 0x98), rt.u64(er + 0xa0)
for n = b, e - 32, 32 do
    local key, pool = rt.u32(n + 8), rt.u64(n + 0x10)
    local vb, ve = rt.u64(pool + 0x50), rt.u64(pool + 0x58)
    if vb and ve and ve > vb and ve - vb <= 0x100 then
        for pg = vb, ve - 8, 8 do
            local page = rt.u64(pg)
            local raw = page and rt.raw(page, 0x20000)
            if raw then
                for i = 1, #raw - 7, 8 do
                    local q = string.unpack("<I8", raw, i)
                    for _, arr in ipairs(arrays) do
                        if q >= arr - 0x10 and q < arr + 0x88 * 18 then out[#out + 1] = string.format("key %08x pool %x page %d off %x -> %x (arr %x %+d)", key, pool, (pg - vb) // 8, i - 1, q, arr, q - arr) end
                    end
                end
            end
        end
    end
end
rt.out("pools6.txt", table.concat(out, "\n"))
rt.log("pools6 fin")
