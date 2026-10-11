rt.run("lib")
local player = rt.u64(BASE + 0x11d61a70)
local opts = rt.u64(rt.u64(player + 0x778) + 0xb8)
local lines = {}
for _, id in ipairs({0x2f, 0x183, 0x27}) do
    local opt = rt.u64(opts + 0x10 + id * 8)
    local hits = rt.watch(opt + 0x18, 2500)
    lines[#lines + 1] = string.format("### option 0x%x value %.3f", id, rt.f32(opt + 0x18) or 0)
    for i = 1, #hits, 3 do
        local rip, n = hits[i], hits[i + 1]
        local f = rt.func(rip)
        lines[#lines + 1] = string.format("== after %x  x%d  func %s", rip, n, f and string.format("rva %x +0x%x", f - BASE, rip - f) or "?")
        if f then lines[#lines + 1] = rt.disasm(math.max(f, rip - 0x40), 22) end
    end
end
rt.out("watch3.txt", table.concat(lines, "\n"))
rt.log("watch3 done")
