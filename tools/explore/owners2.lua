rt.run("lib")
local out = {}
for _, h in ipairs(rt.heap(BASE + 0xe827a70, 20)) do out[#out + 1] = string.format("hand tag at %x (obj %x)", h, h - 0x300) end
for _, h in ipairs(rt.heap(BASE + 0xe7ca730, 20)) do out[#out + 1] = string.format("armor %x", h) end
local player = rt.u64(BASE + 0x11d61a70)
out[#out + 1] = string.format("localplayer %x", player)
for _, a in ipairs({0x196e7bd3ca0, 0x1984cb39ab0}) do
    for _, r in ipairs(rt.heapr(a - 0x10, a + 0x10, 6)) do out[#out + 1] = string.format("ref to attrs %x at %x", a, r) end
end
rt.out("owners2.txt", table.concat(out, "\n"))
rt.log("owners2 fin")
