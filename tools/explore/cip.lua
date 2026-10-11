rt.run("lib")
local ci = 0x196bfb87f70
local player = rt.u64(BASE + 0x11d61a70)
local out = {}
for o = 0, 0x2000 - 8, 8 do if rt.u64(player + o) == ci then out[#out + 1] = string.format("player+0x%x", o) end end
for o = 0, 0x2000 - 8, 8 do local p = rt.u64(player + o); if p and p > 0x10000 then for o2 = 0, 0x400 - 8, 8 do if rt.u64(p + o2) == ci then out[#out + 1] = string.format("player+0x%x>0x%x", o, o2) end end end end
-- global pointers to ci in the image data
for _, s in ipairs(rt.sections()) do end
rt.log("cip", table.concat(out, " "))
