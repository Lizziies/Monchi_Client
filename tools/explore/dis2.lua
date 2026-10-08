rt.run("lib")
local f = BASE + 0x4ba840
local lines = {rt.disasm(f + 0x7f0, 140)}
rt.out("dis2.txt", table.concat(lines, "\n"))
rt.log("dis2 done")
