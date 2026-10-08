rt.run("lib")
local out = {}
for _, a in ipairs({0x4b8210, 0x4b8220, 0x4b8228}) do out[#out + 1] = "### " .. string.format("%x", a); out[#out + 1] = rt.disasm(BASE + a, 40) end
rt.out("dis4.txt", table.concat(out, "\n"))
rt.log("dis4 fin")
