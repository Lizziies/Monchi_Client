rt.run("lib")
local hits = rt.heapi(55, 95, 75, 95, 80, 120, 20000)
local a = {}
for i, h in ipairs(hits) do a[i] = {rt.i32(h), rt.i32(h + 4), rt.i32(h + 8)} end
rt.log("hitres A", #hits)
rt.sleep(5000)
local lines = {}
for i, h in ipairs(hits) do
    local x, y, z = rt.i32(h), rt.i32(h + 4), rt.i32(h + 8)
    local o = a[i]
    if (x ~= o[1] or y ~= o[2] or z ~= o[3]) and x >= 55 and x <= 95 and y >= 75 and y <= 95 and z >= 80 and z <= 120 then
        lines[#lines + 1] = string.format("%x  %d %d %d -> %d %d %d   around: %s", h, o[1], o[2], o[3], x, y, z, rt.hex(h - 0x20, 0x50) or "")
    end
end
rt.out("hitres.txt", table.concat(lines, "\n"))
rt.log("hitres done", #lines)
