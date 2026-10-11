rt.run("lib")
local out = {}
for _, p in ipairs({{0x290e15cf2e0, {0x290ec2ca7a0, 0x290edff24f0, 0x290ee4f13f0}}, {0x29347225b70, {0x290ee4f2be0, 0x290ff68c260, 0x2933f8238b0, 0x29347a378e0}}}) do
    for _, h in ipairs(p[2]) do
        local e = rt.u64(h + 8) or 0
        local line = {}
        for back = 0, 0x300, 8 do local v = rt.u64(h - back); if v and v >= BASE + 0xe000000 and v < BASE + 0xf000000 then line[#line + 1] = string.format("-0x%x:%x", back, v - BASE) end end
        out[#out + 1] = string.format("arr %x holder %x end-start 0x%x | %s", p[1], h, e - p[1], table.concat(line, " "))
    end
end
rt.out("chk5.txt", table.concat(out, "\n"))
rt.log("chk5 fin")
