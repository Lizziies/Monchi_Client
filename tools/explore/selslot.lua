rt.run("lib")
local player = rt.u64(BASE + 0x11d61a70)
local objs = {{"3e8>e00", rt.u64(rt.u64(player + 0x3e8) + 0xe00)}, {"50>e8", rt.u64(rt.u64(player + 0x50) + 0xe8)}, {"3e8", rt.u64(player + 0x3e8)}, {"50", rt.u64(player + 0x50)}, {"player", player}}
local function snap() local s = {} for i, o in ipairs(objs) do s[i] = rt.raw(o[2], 0x2000) or "" end return s end
local a = snap(); rt.log("sel A"); rt.sleep(2500)
local b = snap(); rt.log("sel B"); rt.sleep(2500)
local c = snap()
local out = {}
for i, o in ipairs(objs) do
    for k = 1, math.min(#a[i], #b[i], #c[i]) do
        if a[i]:byte(k) == 0 and b[i]:byte(k) == 1 and c[i]:byte(k) == 6 then out[#out + 1] = string.format("%s+0x%x", o[1], k - 1) end
    end
end
rt.log("sel fin", table.concat(out, " "))
