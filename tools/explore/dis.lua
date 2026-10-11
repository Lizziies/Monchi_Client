rt.run("lib")
rt.run("distag")
local lines = {}
for _, rva in ipairs(DIS) do
    lines[#lines + 1] = string.format("### rva %x", rva)
    lines[#lines + 1] = rt.disfunc(BASE + rva, DISN or 80)
    for _, c in ipairs(rt.callers(BASE + rva, 12) or {}) do
        local f = rt.func(c)
        lines[#lines + 1] = string.format("caller %x  func rva %s", c - BASE, f and string.format("%x", f - BASE) or "?")
    end
end
rt.out("dis.txt", table.concat(lines, "\n"))
rt.log("dis done")
