rt.run("lib")
local player = rt.u64(BASE + 0x11d61a70)
local hr = rt.u64(player + 0x1e8)
local er = rt.u64(hr + 0x38)
local tid = rt.u32(hr + 0x48)
local idx = tid & 0x3ffff
local out = {string.format("target %x idx %x", tid, idx)}
local b, e = rt.u64(er + 0x98), rt.u64(er + 0xa0)
for n = b, e - 32, 32 do
    local key, pool = rt.u32(n + 8), rt.u64(n + 0x10)
    local sb, se = rt.u64(pool + 8), rt.u64(pool + 0x10)
    if sb and se and se > sb and (se - sb) // 8 > idx // 4096 then
        local sp = rt.u64(sb + (idx // 4096) * 8)
        local entry = sp and rt.u32(sp + (idx % 4096) * 4)
        if entry and entry ~= 0xffffffff then
            local pos = entry & 0x3ffff
            local pb = rt.u64(pool + 0x20)
            if rt.u32(pb + pos * 4) == tid then
                -- element: unknown size; read candidate region after page start
                local page = rt.u64(rt.u64(pool + 0x50) or 0)
                local hit = ""
                if page then
                    for es = 8, 0x200, 8 do
                        local el = page + pos * es
                        for k = 0, math.min(es, 0x80) - 8, 8 do
                            local q = rt.u64(el + k)
                            local s1 = rt.cstr(el + k, 40)
                            local s2 = q and q > 0x10000 and q < 0x7fffffffffff and rt.cstr(q, 40)
                            if (s1 and s1:find("armor_stand")) or (s2 and s2:find("armor_stand")) then hit = string.format("elemsize? %x off %x '%s'", es, k, s1 and s1:find("armor") and s1 or s2) break end
                        end
                        if hit ~= "" then break end
                    end
                end
                out[#out + 1] = string.format("key %08x pool %x pos %d %s", key, pool, pos, hit)
            end
        end
    end
end
rt.out("pools8.txt", table.concat(out, "\n"))
rt.log("pools8 fin")
