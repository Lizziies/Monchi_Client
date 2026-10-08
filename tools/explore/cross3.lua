rt.run("lib")
local lines = {}
for _, txt in ipairs({"hud_cursor", "HudCursor", "cursor_renderer", "hud_cursor_renderer", "cross_hair"}) do
    for _, s in ipairs(rt.bytes(txt, false, 8)) do
        local st = s
        for k = 1, 40 do if rt.u8(st - 1) == 0 then break end st = st - 1 end
        lines[#lines + 1] = string.format("[%s] %x: %s", txt, st - BASE, rt.cstr(st, 80) or "")
        for _, x in ipairs(rt.xrefs(st, 6)) do
            local f = rt.func(x)
            lines[#lines + 1] = string.format("   xref %x func %s", x - BASE, f and string.format("%x", f - BASE) or "?")
        end
    end
end
rt.out("cross3.txt", table.concat(lines, "\n"))
rt.log("cross3 fin", #lines)
