rt.run("lib")
rt.run("mkpattern_target")

-- byte patterns for every rip-relative load of TARGET_RVA, displacement masked, checked for exactly one hit
local target = BASE + TARGET_RVA
local lines = {}
for _, ref in ipairs(rt.xrefs(target, 16)) do
    local insn = ref - 3
    for _, before in ipairs({0, 4, 8, 12}) do
        local start = insn - before
        local hex = rt.hex(start, before + 7 + 8)
        if hex then
            local bytes = {}
            for i = 1, #hex, 2 do bytes[#bytes + 1] = hex:sub(i, i + 1) end
            for k = before + 4, before + 7 do bytes[k] = "?" end
            local pattern = table.concat(bytes, " ")
            local hits = rt.find(pattern, 3)
            if #hits == 1 then
                lines[#lines + 1] = string.format("ref %s before %d hits 1 rip offset %d  %s", hexa(insn), before, before + 3, pattern)
                break
            end
        end
    end
end
rt.out("pattern_" .. TAG .. ".txt", table.concat(lines, "\n"))
rt.log("mkpattern done", TAG, #lines)
