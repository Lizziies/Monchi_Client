rt.run("lib")
local vt = BASE + 0xe778140
local out = {string.format("localplayer %x", rt.u64(BASE + 0x11d61a70))}
for _, h in ipairs(rt.heap(BASE + 0x11d1ecf8, 400)) do
    local e = h - 8
    if rt.u64(e) == vt then
        local s = e
        while rt.u64(s - 0x88) == vt do s = s - 0x88 end
        local hp
        for k = 0, 20 do if rt.u64(s + k * 0x88 + 8) == BASE + 0x11dc54f0 then hp = s + k * 0x88 end end
        local refs = #rt.heapr(s, s + 1, 20)
        out[#out + 1] = string.format("array %x hunger %.1f hp %s refs %d", s, rt.f32(e + 0x7c), hp and string.format("%.1f", rt.f32(hp + 0x7c)) or "-", refs)
    end
end
rt.out("hpv2.txt", table.concat(out, "\n"))
rt.log("hpv2 fin")
