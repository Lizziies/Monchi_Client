rt.run("lib")
local hr = rt.u64(rt.u64(BASE + 0x11d61a70) + 0x1e8)
local addr = hr + 0x18
local function collect(hits)
    local s = {}
    for i = 1, #hits, 3 do s[hits[i]] = {n = hits[i + 1], stack = hits[i + 2]} end
    return s
end
local function fmt(v) local g = rt.func(v) return g and string.format("%x(+%x)", g - BASE, v - g) or string.format("?%x", v - BASE) end
rt.log("atk2 type", rt.i32(hr + 0x18))
local a = collect(rt.watch(addr, 2500))
rt.log("atk2 B")
local b = collect(rt.watch(addr, 3000))
local lines = {}
for rip, e in pairs(b) do
    if not a[rip] then
        local chain = {}
        for s in e.stack:gmatch("%x+") do chain[#chain + 1] = fmt(tonumber(s, 16)) end
        lines[#lines + 1] = string.format("NEW %s x%d < %s", fmt(rip), e.n, table.concat(chain, " < "))
    end
end
rt.out("atk2.txt", table.concat(lines, "\n"))
rt.log("atk2 fin", #lines)
