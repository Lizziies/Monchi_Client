rt.run("lib")
local player = rt.u64(BASE + 0x11d61a70)
local X = rt.u64(rt.u64(rt.u64(player + 0x28) + 0x258) + 0x1f0)
local lines = {string.format("X %x", X)}
for k = 0, 7 do
    local h = X + 0x18 + k * 0x88
    lines[#lines + 1] = string.format("+0x%x start %.2f %.2f %.2f dir %.2f %.2f %.2f type %d face %d block %d %d %d pos %.2f %.2f %.2f  rest %s",
        h - X, rt.f32(h), rt.f32(h + 4), rt.f32(h + 8), rt.f32(h + 12), rt.f32(h + 16), rt.f32(h + 20), rt.i32(h + 24), rt.i32(h + 28),
        rt.i32(h + 32), rt.i32(h + 36), rt.i32(h + 40), rt.f32(h + 44), rt.f32(h + 48), rt.f32(h + 52), rt.hex(h + 56, 0x30))
end
rt.out("hits.txt", table.concat(lines, "\n"))
rt.log("hits done")
