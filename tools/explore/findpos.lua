-- position shown by the game: 3, 109, 57 (floored), so floats in [3,4) x [108,112) x [57,58)
local hits = rt.heapf(3, 4, 108, 112, 57, 58, 64)
local lines = {}
for _, h in ipairs(hits) do
    lines[#lines + 1] = string.format("%x  %s %s %s", h, rt.f32(h), rt.f32(h + 4), rt.f32(h + 8))
end
rt.out("findpos.txt", table.concat(lines, "\n"))
rt.log("findpos hits", #hits)
