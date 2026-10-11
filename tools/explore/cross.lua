rt.run("lib")
local lines = {}
for _, txt in ipairs({"cross_hair", "crosshair"}) do
    for _, s in ipairs(rt.bytes(txt, false, 16)) do
        local str = rt.cstr(s - 24, 64) or ""
        lines[#lines + 1] = string.format("str %x (rva %x): %s", s, s - BASE, rt.cstr(s, 60) or "")
        for _, x in ipairs(rt.xrefs(s, 8)) do
            local f = rt.func(x)
            lines[#lines + 1] = string.format("   xref %x func %s", x - BASE, f and string.format("%x", f - BASE) or "?")
        end
    end
end
rt.out("cross.txt", table.concat(lines, "\n"))
rt.log("cross fin", #lines)
