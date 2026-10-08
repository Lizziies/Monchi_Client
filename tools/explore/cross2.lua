rt.run("lib")
local lines = {}
for _, txt in ipairs({"#crosshair", "crosshair_visible", "show_crosshair", "is_crosshair", "crosshair_screen", "hide_crosshair", "#hud_visible"}) do
    for _, s in ipairs(rt.bytes(txt, false, 8)) do
        lines[#lines + 1] = string.format("[%s] %x: %s", txt, s - BASE, rt.cstr(s, 60) or "")
        for _, x in ipairs(rt.xrefs(s, 6)) do
            local f = rt.func(x)
            lines[#lines + 1] = string.format("   xref %x func %s", x - BASE, f and string.format("%x", f - BASE) or "?")
        end
    end
end
rt.out("cross2.txt", table.concat(lines, "\n"))
rt.log("cross2 fin", #lines)
