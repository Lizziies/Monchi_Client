rt.run("lib")
local function name(stack)
    local w = rt.u64(stack + 8); if not w or w < 0x10000 then return "-" end
    local it = rt.u64(w); if not it or it < 0x10000 then return "-" end
    local len = rt.u64(it + 0x128 + 16) or 0
    if len == 0 or len > 64 then return "?" end
    local cap = rt.u64(it + 0x128 + 24) or 0
    return rt.cstr(cap > 15 and rt.u64(it + 0x128) or (it + 0x128), len) or "?"
end
local player = rt.u64(BASE + 0x11d61a70)
local out = {}
local a = rt.u64(player + 0x1068)
for k = -2, 4 do out[#out + 1] = string.format("1068 +0x%x %s", 0xe28 + k * 0x98, name(a + 0xe28 + k * 0x98)) end
local c = rt.u64(rt.u64(player + 0xb0) + 0x648)
for k = -2, 4 do out[#out + 1] = string.format("b0>648 +0x%x %s", 0xc8 + k * 0x98, name(c + 0xc8 + k * 0x98)) end
rt.out("armor2.txt", table.concat(out, "\n"))
rt.log("armor2 fin")
