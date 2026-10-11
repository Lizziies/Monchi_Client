rt.run("lib")
rt.run("region_tag")

-- floats around the position block reached via LocalPlayer global > 0x28 > 0x258 > 0x5e0
local player = rt.u64(BASE + 0x11d61a70)
local r = rt.u64(rt.u64(rt.u64(player + 0x28) + 0x258) + 0x5e0)
local lines = {string.format("player %x block %x", player, r)}
for off = 0x400, 0x900, 4 do
    lines[#lines + 1] = string.format("+%03x %s", off, tostring(rt.f32(r + off)))
end
-- the player object itself, for fields stored directly in it
for off = 0, 0x1800, 4 do
    lines[#lines + 1] = string.format("p+%04x %s", off, tostring(rt.f32(player + off)))
end
rt.out("region_" .. TAG .. ".txt", table.concat(lines, "\n"))
rt.log("region done", TAG)
