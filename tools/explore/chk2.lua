rt.run("lib")
local out = {}
for _, h in ipairs({0x29345a38c28, 0x290e08ed6d0, 0x290ee6640c0, 0x290ed1df3b8, 0x290f1ec0140, 0x290dada3bb0}) do
    local line = {}
    for back = 0x0, 0x300, 8 do
        local v = rt.u64(h - back)
        if v and v >= BASE and v < BASE + 0x14000000 then line[#line + 1] = string.format("-0x%x:rva %x", back, v - BASE) end
    end
    out[#out + 1] = string.format("%x  %s", h, table.concat(line, " "))
end
rt.out("chk2.txt", table.concat(out, "\n"))
rt.log("chk2 fin")
