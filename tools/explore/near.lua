rt.run("lib")
local player = rt.u64(BASE + 0x11d61a70)
local lines = {}
for off = 0x1a0, 0x260, 8 do
    local p = rt.u64(player + off)
    if p and p > 0x10000 then
        local fl = {}
        for k = 0, 15 do local v = rt.f32(p + k * 4); fl[#fl + 1] = v and string.format("%.2f", v) or "?" end
        lines[#lines + 1] = string.format("+0x%x -> %x : %s", off, p, table.concat(fl, " "))
    end
end
rt.out("near.txt", table.concat(lines, "\n"))
rt.log("near done")
