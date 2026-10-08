rt.run("lib")
local hits = rt.heapf(34.9, 36.1, 108.9, 111.8, 75.9, 77.1, 300)
local before = {}
for i, a in ipairs(hits) do before[i] = {rt.f32(a), rt.f32(a + 4), rt.f32(a + 8)} end
rt.log("heappos A", #hits)
rt.sleep(7000)
local lines = {}
for i, a in ipairs(hits) do
    local x, y, z = rt.f32(a), rt.f32(a + 4), rt.f32(a + 8)
    local b = before[i]
    lines[#lines + 1] = string.format("%s %x  %.3f %.3f %.3f -> %.3f %.3f %.3f", (math.abs(x - b[1]) + math.abs(z - b[3]) > 1) and "MOVED" or "still", a, b[1], b[2], b[3], x, y, z)
end
table.sort(lines)
rt.out("heappos.txt", table.concat(lines, "\n"))
rt.log("heappos done")
