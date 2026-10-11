rt.run("lib")
local tag = 0x28c3f1f6458
local lines = {}
for k = -0x60, 0x2f8, 8 do
    lines[#lines + 1] = string.format("%5d  %016x  %08x=%.4f  %08x=%.4f", k, rt.u64(tag + k) or 0, rt.u32(tag + k) or 0, rt.f32(tag + k) or 0, rt.u32(tag + k + 4) or 0, rt.f32(tag + k + 4) or 0)
end
rt.out("option3.txt", table.concat(lines, "\n"))
rt.log("option3 done")
