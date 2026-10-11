rt.run("lib")
local vt = 0x7ff7437f8ef0
local lines = {}
local slots = rt.vtable(vt, 40)
for i, f in ipairs(slots) do
    local d = rt.disfunc(f, 24) or ""
    if d:find("0x18%]") or d:find("0x10%]") or d:find("0x14%]") then
        lines[#lines + 1] = string.format("== slot %d (+0x%x) %x", i - 1, (i - 1) * 8, f)
        lines[#lines + 1] = d
    else
        lines[#lines + 1] = string.format("   slot %d %x", i - 1, f)
    end
end
rt.out("fopt.txt", table.concat(lines, "\n"))
rt.log("fopt done")
