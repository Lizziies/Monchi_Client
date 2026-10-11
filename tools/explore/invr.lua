rt.run("lib")
local player = rt.u64(BASE + 0x11d61a70)
local pinv = rt.u64(player + 0x350)
local cont = rt.u64(pinv + 0x378)
local b = rt.u64(cont + 0x198)
local c = {}
for k = 0, 8 do c[#c + 1] = tostring(rt.u8(b + k * 0x98 + 0x22)) end
local raw = {}
for o = 0, 0x40, 4 do raw[#raw + 1] = string.format("%x:%d", o, rt.i32(pinv + o) or -1) end
rt.log("invr", string.format("pinv %x cont %x", pinv, cont), table.concat(c, ","), table.concat(raw, " "))
