rt.run("lib")
local player = rt.u64(BASE + 0x11d61a70)
local holders = {0x29345a38c28, 0x290e08ed6d0, 0x290ee6640c0, 0x290ed1df3b8, 0x290f1ec0140, 0x290dada3bb0}
local out = {}
local p350 = rt.u64(player + 0x350)
out[#out + 1] = string.format("player %x  +350 %x  +350>378 %x", player, p350 or 0, p350 and rt.u64(p350 + 0x378) or 0)
-- any holder within 0x2000 after a pointer stored in the player or one level below
for o = 0, 0x2000 - 8, 8 do
    local a = rt.u64(player + o)
    if a and a > 0x10000 then
        for _, h in ipairs(holders) do if h >= a and h < a + 0x2000 then out[#out + 1] = string.format("player+0x%x -> +0x%x", o, h - a) end end
        for o2 = 0, 0x1000 - 8, 8 do
            local b = rt.u64(a + o2)
            if b and b > 0x10000 then
                for _, h in ipairs(holders) do if h >= b and h < b + 0x400 then out[#out + 1] = string.format("player+0x%x>0x%x -> +0x%x", o, o2, h - b) end end
            end
        end
    end
end
rt.out("chk.txt", table.concat(out, "\n"))
rt.log("chk fin")
