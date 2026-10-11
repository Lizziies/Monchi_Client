rt.run("lib")
local a = {}
for _, h in ipairs(rt.heapf(0.5295, 0.5305, 4000)) do a[#a + 1] = {h, "frac"} end
for _, h in ipairs(rt.heapf(52.95, 53.05, 4000)) do a[#a + 1] = {h, "pct"} end
rt.log("gamma A", #a)
rt.sleep(12000)
local lines = {}
for _, x in ipairs(a) do
    local v = rt.f32(x[1])
    if v and math.abs(v - 0.53) > 0.005 and math.abs(v - 53) > 0.05 then lines[#lines + 1] = string.format("%s %x -> %.4f", x[2], x[1], v) end
end
rt.out("gamma.txt", table.concat(lines, "\n"))
rt.log("gamma done", #lines)
