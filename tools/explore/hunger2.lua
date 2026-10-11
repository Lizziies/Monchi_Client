rt.run("lib")
local vt = BASE + 0xe778140
local out = {}
for _, d in ipairs({{"health", 0x11dc54f0}, {"hunger", 0x11d1ecf8}}) do
    for _, h in ipairs(rt.heap(BASE + d[2], 400)) do
        local e = h - 8
        if rt.u64(e) == vt then out[#out + 1] = string.format("%s inst %x cur %.2f max %.2f", d[1], e, rt.f32(e + 0x7c), rt.f32(e + 0x78)) end
    end
end
rt.out("hunger2.txt", table.concat(out, "\n"))
rt.log("hunger2 fin")
