rt.run("lib")
-- options keep their save tag as a std::string; short tags sit inline (SSO), so the heap holds the text itself
local lines = {}
for _, a in ipairs(rt.heap(0x6d6d61675f786667, 16)) do
    if rt.u8(a + 8) == 0x61 and rt.u8(a + 9) == 0 then
        lines[#lines + 1] = string.format("tag at %x", a)
        for k = -0x100, 0x100, 4 do
            local f = rt.f32(a + k)
            local q = rt.u64(a + k)
            if k % 8 == 0 then
                lines[#lines + 1] = string.format("  %5d  f=%-12.4f u64=%x", k, f or 0, q or 0)
            else
                lines[#lines + 1] = string.format("  %5d  f=%-12.4f", k, f or 0)
            end
        end
    end
end
rt.out("option.txt", table.concat(lines, "\n"))
rt.log("option done", #lines)
