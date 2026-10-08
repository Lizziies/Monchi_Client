rt.run("lib")
local S0 = 0x210c56095f8 local S = S0 while rt.u64(S - 0x88) == rt.u64(S0) and rt.u64(rt.u64(S - 0x88 + 8) or 0) ~= nil do S = S - 0x88 end
local vt = rt.u64(S)
local out = {}
local function nm(def)
    for o = 0, 0x60, 8 do
        local p = def + o
        local len = rt.u64(p + 16) or 0
        if len > 3 and len < 64 then
            local cap = rt.u64(p + 24) or 0
            local s = rt.cstr(cap > 15 and rt.u64(p) or p, len)
            if s and s:find("minecraft:") then return string.format("+%x %s", o, s) end
        end
    end
    return "?"
end
local i = 0
while rt.u64(S + i * 0x88) == vt and i < 40 do
    local e = S + i * 0x88
    local def = rt.u64(e + 8)
    out[#out + 1] = string.format("%2d def rva %x  %s  cur %.3f max %.3f", i, def - BASE, nm(def), rt.f32(e + 0x7c), rt.f32(e + 0x78))
    i = i + 1
end
rt.out("attrs.txt", table.concat(out, "\n"))
rt.log("attrs fin")
