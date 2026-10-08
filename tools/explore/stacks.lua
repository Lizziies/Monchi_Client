rt.run("lib")
local lines = {}
for _, vt in ipairs({0x7ff7af986cd0, 0x7ff7af986d30, 0x7ff7afac6a40}) do local rva = vt - BASE
    local hits = rt.heap(vt, 200000)
    table.sort(hits)
    lines[#lines + 1] = string.format("vt %x: %d", rva, #hits)
    local i = 1
    while i < #hits do
        local stride = hits[i + 1] - hits[i]
        local j = i + 1
        while j < #hits and hits[j + 1] - hits[j] == stride do j = j + 1 end
        local run = j - i + 1
        if run >= 9 and stride <= 0x200 then
            local cnts = {}
            for k = 0, math.min(run, 12) - 1 do cnts[#cnts + 1] = tostring(rt.u8(hits[i] + k * stride + 0x22)) end
            lines[#lines + 1] = string.format("  run %d stride 0x%x at %x  counts %s", run, stride, hits[i], table.concat(cnts, ","))
        end
        i = j + 1
    end
end
rt.out("stacks.txt", table.concat(lines, "\n"))
rt.log("stacks fin")
