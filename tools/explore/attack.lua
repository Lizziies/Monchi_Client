rt.run("lib")
local player = rt.u64(BASE + 0x11d61a70)
local X = rt.u64(rt.u64(rt.u64(player + 0x28) + 0x258) + 0x1f0)
local addr = X + 0x18 + 0x18
local function collect(hits)
    local s = {}
    for i = 1, #hits, 3 do s[hits[i]] = {n = hits[i + 1], stack = hits[i + 2]} end
    return s
end
local function fmt(v)
    local g = rt.func(v)
    return g and string.format("%x(+%x)", g - BASE, v - g) or string.format("?%x", v - BASE)
end
local a = collect(rt.watch(addr, 2500))
rt.log("attack B")
local b = collect(rt.watch(addr, 3000))
local lines = {string.format("type at %x", addr)}
for rip, e in pairs(b) do
    local chain = {}
    for s in e.stack:gmatch("%x+") do chain[#chain + 1] = fmt(tonumber(s, 16)) end
    lines[#lines + 1] = string.format("%s %s x%d  < %s", a[rip] and "both" or "NEW ", fmt(rip), e.n, table.concat(chain, " < "))
end
table.sort(lines)
rt.out("attack.txt", table.concat(lines, "\n"))
rt.log("attack done")
