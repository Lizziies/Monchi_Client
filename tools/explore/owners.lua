rt.run("lib")
local vt = BASE + 0xe6d6cd0
local function name(stack)
    local w = rt.u64(stack + 8); if not w or w < 0x10000 then return "" end
    local it = rt.u64(w); if not it or it < 0x10000 then return "" end
    local len = rt.u64(it + 0x128 + 16) or 0
    if len == 0 or len > 64 then return "" end
    local cap = rt.u64(it + 0x128 + 24) or 0
    return rt.cstr(cap > 15 and rt.u64(it + 0x128) or (it + 0x128), len) or ""
end
local out = {}
for _, s in ipairs(rt.heap(vt, 20000)) do
    local n = name(s)
    if n:find("chestplate") or n:find("shield") then
        local line = {}
        for back = 0x8, 0x600, 8 do
            local v = rt.u64(s - back)
            if v and v >= BASE + 0xe000000 and v < BASE + 0xf000000 and v ~= vt then line[#line + 1] = string.format("-0x%x:%x", back, v - BASE) end
        end
        out[#out + 1] = string.format("%x %s | %s", s, n, table.concat(line, " "))
    end
end
rt.out("owners.txt", table.concat(out, "\n"))
rt.log("owners fin")
