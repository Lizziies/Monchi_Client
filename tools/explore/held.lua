rt.run("lib")
local player = rt.u64(BASE + 0x11d61a70)
local b = rt.u64(rt.u64(rt.u64(player + 0x350) + 0x378) + 0x198)
local item0, item1 = rt.u64(b + 8), rt.u64(b + 0x98 + 8)
local vt = rt.u64(b)
local all = rt.heap(vt, 5000)
local snapA = {}
for _, s in ipairs(all) do snapA[s] = rt.u64(s + 8) end
rt.log("held A", #all, string.format("%x %x", item0, item1))
rt.sleep(3000)
local out = {}
for _, s in ipairs(all) do
    if snapA[s] == item0 and rt.u64(s + 8) == item1 then out[#out + 1] = string.format("%x", s) end
end
rt.log("held fin", table.concat(out, " "))
