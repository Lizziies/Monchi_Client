rt.run("lib")
rt.run("watchtag")
local hits = rt.watch(WATCH, WATCHMS or 3000, WATCHWRITES)
local lines = {string.format("watch %x", WATCH)}
for i = 1, #hits, 3 do
    local rip, n, stack = hits[i], hits[i + 1], hits[i + 2]
    local f = rt.func(rip)
    lines[#lines + 1] = string.format("== after %x  x%d  func %s", rip, n, f and string.format("rva %x +0x%x", f - BASE, rip - f) or "?")
    local chain = {}
    for a in stack:gmatch("%x+") do
        local v = tonumber(a, 16)
        local g = rt.func(v)
        chain[#chain + 1] = g and string.format("%x(+%x)", g - BASE, v - g) or string.format("?%x", v - BASE)
    end
    lines[#lines + 1] = "   stack " .. table.concat(chain, " < ")
    if f and not NODIS then lines[#lines + 1] = rt.disasm(math.max(f, rip - 0x30), 14) end
end
rt.out("watch.txt", table.concat(lines, "\n"))
rt.log("watch done", #hits // 3)
