rt.run("lib")
local sv = rt.u64(rt.u64(BASE + 0x11d61a70) + 0x1e8)
local x, ey, z = rt.f32(sv), rt.f32(sv + 4), rt.f32(sv + 8)
local fy = ey - 1.62
local out = {string.format("eye %.3f %.3f %.3f", x, ey, z)}
for _, a in ipairs(rt.heapf(x - 0.31, x - 0.29, fy - 0.01, fy + 0.01, z - 0.31, z - 0.29, 200)) do
    local f = {}
    for k = 0, 9 do f[#f + 1] = string.format("%.3f", rt.f32(a + k * 4) or 0) end
    out[#out + 1] = string.format("%x : %s", a, table.concat(f, " "))
end
rt.out("aabb.txt", table.concat(out, "\n"))
rt.log("aabb fin")
