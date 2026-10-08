rt.run("lib")
local er = rt.u64(rt.u64(BASE + 0x11d61a70) + 0x320)
local function poolOf(key)
    local b, e = rt.u64(er + 0x98), rt.u64(er + 0xa0)
    for n = b, e - 32, 32 do if rt.u32(n + 8) == key then return rt.u64(n + 0x10) end end
end
local p = poolOf(0x4f6047c7)
local page = rt.u64(rt.u64(p + 0x50))
local out = {}
for k = 0, 0x800, 8 do
    local l, c = rt.u64(page + k + 16), rt.u64(page + k + 24)
    if l and c and l > 3 and l < 64 and c >= l and c < 0x100 then
        local s = rt.cstr(c > 15 and rt.u64(page + k) or (page + k), l)
        if s and s:match("^[%w_:%.]+$") then out[#out + 1] = string.format("+%x '%s'", k, s) end
    end
end
local gc = poolOf(0x9ec7d9e5)
out[#out + 1] = string.format("game camera entity %x", rt.u32(rt.u64(gc + 0x20)))
rt.out("cam2.txt", table.concat(out, "\n"))
rt.log("cam2 fin")
