rt.run("lib")
local player = rt.u64(BASE + 0x11d61a70)
local lines = {}
for _, off in ipairs({0x60, 0x90, 0x2e0, 0x408, 0x420, 0x4e0, 0x130, 0x158}) do
    local o = rt.u64(player + off)
    local vt = o and rt.u64(o) or 0
    lines[#lines + 1] = string.format("player+0x%x -> %x  vtable %x (rva %x)", off, o or 0, vt, vt - BASE)
end
lines[#lines + 1] = rt.disfunc(BASE + 0x1743af0, 30)
rt.out("lvl.txt", table.concat(lines, "\n"))
rt.log("lvl done")
