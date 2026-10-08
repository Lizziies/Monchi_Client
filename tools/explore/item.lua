rt.run("lib")
-- "wheat_se" as little-endian u64, then "eds" and a zero
local lines = {}
for _, a in ipairs(rt.heap(0x65735f7461656877, 64)) do
    if rt.u8(a + 8) == 0x65 and rt.u8(a + 9) == 0x64 and rt.u8(a + 10) == 0x73 and rt.u8(a + 11) == 0 then
        lines[#lines + 1] = string.format("str %x  len %d", a, rt.u64(a + 16) or -1)
        -- who points into the 0x200 bytes before the string (the owning object)
        for back = 0, 0x200, 8 do
            for _, r in ipairs(rt.heap(a - back, 4)) do
                lines[#lines + 1] = string.format("   ref to str-0x%x at %x", back, r)
            end
        end
    end
end
rt.out("item.txt", table.concat(lines, "\n"))
rt.log("item done", #lines)
