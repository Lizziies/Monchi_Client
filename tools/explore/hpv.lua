rt.run("lib")
local out = {}
for _, h in ipairs(rt.heap(BASE + 0x11dc54f0, 400)) do
    local e = h - 8
    if rt.u64(e) == BASE + 0xe778140 and rt.u64(e - 5 * 0x88 + 8) == BASE + 0x11d1ecf8 then
        out[#out + 1] = string.format("%x hp %.1f/%.1f", e, rt.f32(e + 0x7c), rt.f32(e + 0x78))
    end
end
rt.log("hpv", table.concat(out, " | "))
