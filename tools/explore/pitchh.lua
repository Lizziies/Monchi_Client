rt.run("lib")
local hits = rt.heapf(85, 91, 20000)
rt.log("pitchh A", #hits)
rt.sleep(5000)
local lines = {}
for _, a in ipairs(hits) do
    local v = rt.f32(a)
    if v and v < -85 and v > -91 then
        lines[#lines + 1] = string.format("%x  now %.3f  next %.3f  prev %.3f %.3f %.3f", a, v, rt.f32(a + 4), rt.f32(a - 12), rt.f32(a - 8), rt.f32(a - 4))
    end
end
rt.out("pitchh.txt", table.concat(lines, "\n"))
rt.log("pitchh done", #lines)
