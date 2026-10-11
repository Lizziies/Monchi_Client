rt.run("lib")
local lines = {}
local hits = rt.find("48 8B ?? 98 01 00 00", 20000)
lines[#lines + 1] = "loads " .. #hits
for _, h in ipairs(hits) do
    local hx = rt.hex(h + 7, 40) or ""
    -- movss xmm,[r+18] with optional REX prefix
    local at = hx:find("f30f10%x%x18") or hx:find("f3%x%x0f10%x%x18")
    if at and at % 2 == 1 and at < 60 then
        local f = rt.func(h)
        lines[#lines + 1] = string.format("%x func %s  %s", h, f and string.format("%x", f) or "?", hx:sub(1, 48))
    end
end
rt.out("gread3.txt", table.concat(lines, "\n"))
rt.log("gread3 done", #lines)
