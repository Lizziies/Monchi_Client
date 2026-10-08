rt.run("lib")
local a = 0xe8d6efea60
local s = {}
for k = 1, 12 do s[#s + 1] = tostring(rt.u8(a)); rt.sleep(500) end
rt.log("slotv", table.concat(s, " "), rt.hex(a - 0x40, 0x80))
