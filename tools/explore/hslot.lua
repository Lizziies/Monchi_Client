rt.run("lib")
local objs = {}
for _, h in ipairs(rt.heap(BASE + 0xe827a70, 20)) do
    if rt.u64(h + 8) == BASE + 0xe827aa0 then objs[#objs + 1] = h - 0x300 end
end
local function snap() local s = {} for i, o in ipairs(objs) do s[i] = rt.raw(o - 0x400, 0x1000) or "" end return s end
local a = snap(); rt.log("hs A", #objs); rt.sleep(2500)
local b = snap(); rt.log("hs B"); rt.sleep(2500)
local c = snap(); rt.log("hs C"); rt.sleep(2500)
local d = snap()
local out = {}
for i, o in ipairs(objs) do
    for k = 1, math.min(#a[i], #b[i], #c[i], #d[i]) do
        if a[i]:byte(k) == 0 and b[i]:byte(k) == 1 and c[i]:byte(k) == 6 and d[i]:byte(k) == 3 then out[#out + 1] = string.format("obj%d %x +0x%x", i, o, k - 1 - 0x400) end
    end
end
rt.log("hs fin", table.concat(out, " "))
