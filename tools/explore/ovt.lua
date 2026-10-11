rt.run("lib")
local lines = {}
for _, vt in ipairs({0x7ff74387c490, 0x7ff74387cf40}) do
    local slots = rt.vtable(vt, 400)
    lines[#lines + 1] = string.format("vtable %x slots %d", vt, #slots)
    for i, f in ipairs(slots) do
        local ok, d = pcall(rt.disfunc, f, 10); if not ok then d = "" end
        if d:find("%*8") or d:find("0x18%]") or d:find("0x1a0%]") or d:find("0x198%]") then
            lines[#lines + 1] = string.format("== slot %d %x", i - 1, f)
            lines[#lines + 1] = d
        end
    end
end
rt.out("ovt.txt", table.concat(lines, "\n"))
rt.log("ovt done")
