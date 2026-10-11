rt.run("lib")
local c = {0x196bf9622b0, 0x196d882b320, 0x196f7d03490, 0x196f7faae70, 0x196f7fac190, 0x196f7fb07e0, 0x196f7fb0bd0, 0x196f88bc740, 0x1983e04bc50}
local player = rt.u64(BASE + 0x11d61a70)
local out = {}
for o = 0, 0x2000 - 8, 8 do
    local p = rt.u64(player + o)
    if p and p > 0x10000 then
        for _, t in ipairs(c) do if t >= p and t < p + 0x4000 then out[#out + 1] = string.format("player+0x%x -> %x +0x%x", o, p, t - p) end end
        for o2 = 0, 0x800 - 8, 8 do
            local q = rt.u64(p + o2)
            if q and q > 0x10000 then
                for _, t in ipairs(c) do if t >= q and t < q + 0x4000 then out[#out + 1] = string.format("player+0x%x>0x%x -> %x +0x%x", o, o2, q, t - q) end end
            end
        end
    end
end
rt.out("tpath.txt", table.concat(out, "\n"))
rt.log("tpath fin", #out)
