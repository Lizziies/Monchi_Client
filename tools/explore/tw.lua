rt.run("lib")
local player = rt.u64(BASE + 0x11d61a70)
local v = rt.i32(rt.u64(player + 0x90) + 0x340)
rt.scanstart(4, v, v)
local cands = rt.scanlist(20)
local out = {"time " .. v .. " copies " .. #cands}
for _, a in ipairs(cands) do
    local hits = rt.watch(a, 700)
    out[#out + 1] = string.format("## %x reads %d", a, #hits // 3)
    for i = 1, #hits, 3 do
        local f = rt.func(hits[i])
        out[#out + 1] = string.format("   %s x%d < %s", f and string.format("%x(+%x)", f - BASE, hits[i] - f) or "?", hits[i + 1], hits[i + 2])
    end
end
rt.out("tw.txt", table.concat(out, "\n"))
rt.log("tw fin")
