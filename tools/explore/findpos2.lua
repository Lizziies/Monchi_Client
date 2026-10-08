-- every float triple in writable memory that matches the shown block position (feet and eye height)
local function run(tag, ylo, yhi)
    local hits = rt.heapf(3, 4, ylo, yhi, 57, 58, 200)
    local lines = {}
    for _, h in ipairs(hits) do
        lines[#lines + 1] = string.format("%s %x  %s %s %s", tag, h, rt.f32(h), rt.f32(h + 4), rt.f32(h + 8))
    end
    rt.log("findpos2", tag, #hits)
    return lines
end
local out = {}
for _, l in ipairs(run("feet", 109, 110)) do out[#out + 1] = l end
for _, l in ipairs(run("eye", 110.5, 111.7)) do out[#out + 1] = l end
rt.out("findpos2.txt", table.concat(out, "\n"))
