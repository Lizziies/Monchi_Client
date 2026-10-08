rt.run("lib")
local out = {}
local pl = rt.u64(BASE + 0x11d61a70)
local level = rt.u64(pl + 0x90)
for _, f in ipairs({0x20792a26508, 0x2078c6e1a48}) do
    local refs = rt.heapr(f - 0x200, f, 30)
    for _, r in ipairs(refs) do
        local v = rt.u64(r)
        out[#out + 1] = string.format("field %x <- %x (value %x, field-value %x) level+%x vt %x", f, r, v, f - v, r - level, (rt.u64(v) or 0) - BASE)
    end
end
for o = 0, 0x3000, 8 do
    local p = rt.u64(level + o)
    for _, f in ipairs({0x20792a26508, 0x2078c6e1a48}) do
        if p and f >= p and f - p < 0x400 then out[#out + 1] = string.format("level+%x -> %x (+%x)", o, p, f - p) end
    end
end
rt.out("sb3.txt", table.concat(out, "\n"))
rt.log("sb3 fin")
