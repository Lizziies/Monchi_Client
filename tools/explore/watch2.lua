rt.run("lib")
local player = rt.u64(BASE + 0x11d61a70)
local lines = {}
for _, spec in ipairs({{"daytime", rt.u64(rt.u64(player + 0x408) + 0xa88) + 0xa0}, {"tick", rt.u64(player + 0x2e0) + 0x10}}) do
    local before = rt.i32(spec[2])
    local hits = rt.watch(spec[2], 2000)
    lines[#lines + 1] = string.format("### %s %x  %d -> %d  hits %d", spec[1], spec[2], before, rt.i32(spec[2]), #hits // 3)
    for i = 1, #hits, 3 do
        local f = rt.func(hits[i])
        local chain = {}
        for a in hits[i + 2]:gmatch("%x+") do local v = tonumber(a, 16); local g = rt.func(v); chain[#chain + 1] = g and string.format("%x(+%x)", g - BASE, v - g) or "?" end
        lines[#lines + 1] = string.format("  %s x%d < %s", f and string.format("%x(+%x)", f - BASE, hits[i] - f) or "?", hits[i + 1], table.concat(chain, " < "))
    end
end
rt.out("watch2.txt", table.concat(lines, "\n"))
rt.log("watch2 done")
