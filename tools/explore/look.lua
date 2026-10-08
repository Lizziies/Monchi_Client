rt.run("lib")
local player = rt.u64(BASE + 0x11d61a70)
local sv = rt.u64(player + 0x1e8)
local px, py, pz = rt.f32(sv), rt.f32(sv + 4), rt.f32(sv + 8)
local list = rt.u64(player + 0x138)
local lines = {string.format("pos %.3f %.3f %.3f list %x", px, py, pz, list or 0)}
for o = 0, 0x2000 - 8, 8 do
    local c = rt.u64(list + o)
    if c and c > 0x10000 then
        local x, y, z = rt.f32(c), rt.f32(c + 4), rt.f32(c + 8)
        if x and math.abs(x - px) < 0.05 and math.abs(y - py) < 0.05 and math.abs(z - pz) < 0.05 then
            local f = {}
            for k = 3, 12 do f[#f + 1] = string.format("%.3f", rt.f32(c + 4 * k) or 0) end
            lines[#lines + 1] = string.format("slot 0x%x -> %x : %s", o, c, table.concat(f, " "))
        end
    end
end
rt.out("look.txt", table.concat(lines, "\n"))
rt.log("look fin")
