rt.run("lib")
local player = rt.u64(BASE + 0x11d61a70)
local b = rt.u64(rt.u64(rt.u64(player + 0x350) + 0x378) + 0x198)
local vt = rt.u64(b)
local function name(stack)
    local w = rt.u64(stack + 8); if not w or w < 0x10000 then return "" end
    local it = rt.u64(w); if not it or it < 0x10000 then return "" end
    local len = rt.u64(it + 0x128 + 16) or 0
    if len == 0 or len > 64 then return "" end
    local cap = rt.u64(it + 0x128 + 24) or 0
    return rt.cstr(cap > 15 and rt.u64(it + 0x128) or (it + 0x128), len) or ""
end
local out = {}
for _, s in ipairs(rt.heap(vt, 5000)) do
    local n = name(s)
    if n:find("chestplate") then out[#out + 1] = string.format("%x %s count %d prev %s next %s", s, n, rt.u8(s + 0x22), name(s - 0x98), name(s + 0x98)) end
end
rt.out("armor.txt", table.concat(out, "\n"))
rt.log("armor fin", #out)
