rt.run("lib")
local player = rt.u64(BASE + 0x11d61a70)
local sv = rt.u64(player + 0x1e8)
local ex, ey, ez = rt.f32(sv), rt.f32(sv + 4), rt.f32(sv + 8)
local lines = {string.format("eye %.3f %.3f %.3f", ex, ey, ez)}
for _, a in ipairs(rt.heapf(ex - 0.02, ex + 0.02, ey - 0.02, ey + 0.02, ez - 0.02, ez + 0.02, 400)) do
    local t = rt.i32(a + 0x18)
    local dx, dy, dz = rt.f32(a + 12), rt.f32(a + 16), rt.f32(a + 20)
    if t and t >= 0 and t <= 3 and dx and math.abs(dx) + math.abs(dy) + math.abs(dz) > 1 then
        lines[#lines + 1] = string.format("%x type %d face %d block %d %d %d hit %.2f %.2f %.2f  ref %x %x  box %s", a, t, rt.i32(a + 0x1c),
            rt.i32(a + 0x20), rt.i32(a + 0x24), rt.i32(a + 0x28), rt.f32(a + 0x2c), rt.f32(a + 0x30), rt.f32(a + 0x34), rt.u64(a + 0x38) or 0, rt.u64(a + 0x40) or 0, rt.hex(a + 0x48, 24) or "")
    end
end
rt.out("hr.txt", table.concat(lines, "\n"))
rt.log("hr done", #lines)
