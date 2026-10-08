rt.run("lib")
local lines = {}
local calls = {}
for _, h in ipairs(rt.find("BA 32 00 00 00", 20000)) do
    local hx = rt.hex(h + 5, 48) or ""
    local at = hx:find("f30f10%x%x18")
    if at and at % 2 == 1 then
        local c = hx:find("e8")
        local tgt = "?"
        if c and c % 2 == 1 and c < at then
            local rel = rt.i32(h + 5 + (c - 1) // 2 + 1)
            tgt = string.format("%x", h + 5 + (c - 1) // 2 + 5 + rel)
            calls[tgt] = (calls[tgt] or 0) + 1
        end
        lines[#lines + 1] = string.format("%x func %x call %s  %s", h, rt.func(h) or 0, tgt, hx:sub(1, 64))
    end
end
for t, n in pairs(calls) do lines[#lines + 1] = string.format("CALLEE %s x%d", t, n) end
rt.out("gread2.txt", table.concat(lines, "\n"))
rt.log("gread2 done", #lines)
