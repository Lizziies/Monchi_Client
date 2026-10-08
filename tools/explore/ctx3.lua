rt.run("lib")
local player = rt.u64(BASE + 0x11d61a70)
local er = rt.u64(rt.u64(player + 0x1e8) + 0x38)
local out = {string.format("player %x er %x", player, er)}
for k = 0, 0x2000 - 8, 8 do
    local q = rt.u64(player + k)
    if q == er or q == er + 0x30 then out[#out + 1] = string.format("player+%x = %x  next %x %x", k, q, rt.u64(player + k + 8) or 0, rt.u64(player + k + 16) or 0) end
end
-- all registries (by name string object) : find objects whose +0x10 string reads MinecraftRegistry
rt.out("ctx3.txt", table.concat(out, "\n"))
rt.log("ctx3 fin")
