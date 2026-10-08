local BREAK = true
rt.run("lib")
local er = rt.u64(rt.u64(BASE + 0x11d61a70) + 0x320)
local function poolOf(key)
    local b, e = rt.u64(er + 0x98), rt.u64(er + 0xa0)
    for n = b, e - 32, 32 do if rt.u32(n + 8) == key then return rt.u64(n + 0x10) end end
end
local cam = rt.u64(rt.u64(poolOf(0x4f6047c7) + 0x50))
local c9 = cam + 7 * 0x120
local bobp = poolOf(0xc7b5d956)
local sp = rt.u64(rt.u64(bobp + 8))
local saved = {}
if BREAK then for _, id in ipairs({2, 3, 4}) do saved[id] = rt.u32(sp + id * 4) rt.wf32(sp + id * 4, string.unpack("<f", string.pack("<I4", saved[id] ~ 0x40000))) end end
local lo, hi, s = 1e9, -1e9, ""
for i = 1, 60 do
    local y = rt.f32(c9 + 0x44)
    lo = math.min(lo, y) hi = math.max(hi, y)
    if i % 4 == 0 then s = s .. string.format(" %.3f", y) end
    rt.sleep(30)
end
for id, v in pairs(saved) do rt.wf32(sp + id * 4, string.unpack("<f", string.pack("<I4", v))) end
rt.log(string.format("bob break=%s range %.4f :%s", tostring(BREAK), hi - lo, s))




