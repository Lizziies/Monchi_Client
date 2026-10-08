rt.run("lib")
local f = BASE + 0x4278cf0
local lines = {}
for _, off in ipairs({0xf67, 0x11e0, 0x14e5, 0x15ab, 0x181f, 0x18dc}) do
    lines[#lines + 1] = string.format("### +0x%x", off)
    lines[#lines + 1] = rt.disasm(f + off - 0x18, 28)
end
rt.out("dis3.txt", table.concat(lines, "\n"))
rt.log("dis3 done")
