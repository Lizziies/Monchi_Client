DEPTH = 3
MAXN = 25000
rt.run("lib")
local player = rt.u64(BASE + 0x11d61a70)
local nodes, seen = {{addr = player, path = "player", depth = 0}}, {[player] = true}
local head = 1
while head <= #nodes and #nodes < (MAXN or 5000) do
    local n = nodes[head]; head = head + 1
    if n.depth < (DEPTH or 2) then
        for _, p in ipairs(rt.pointers(n.addr, n.depth == 0 and 0x1800 or 0x600)) do
            if not seen[p[2]] then
                seen[p[2]] = true
                nodes[#nodes + 1] = {addr = p[2], path = string.format("%s>0x%x", n.path, p[1]), depth = n.depth + 1}
            end
        end
    end
end
local function snap()
    local s = {}
    for i, n in ipairs(nodes) do s[i] = rt.raw(n.addr, n.depth == 0 and 0x1800 or 0x600) or "" end
    return s
end
local a = snap(); rt.log("slotb phaseA"); rt.sleep(3000)
local b = snap(); rt.log("slotb phaseB"); rt.sleep(3000)
local c = snap()
local lines = {}
for i, n in ipairs(nodes) do
    local len = math.min(#a[i], #b[i], #c[i])
    for off = 1, len do
        if a[i]:byte(off) == 0 and b[i]:byte(off) == 2 and c[i]:byte(off) == 5 then
            lines[#lines + 1] = string.format("%s +0x%x  (%x)", n.path, off - 1, n.addr + off - 1)
        end
    end
end
rt.out("slot.txt", table.concat(lines, "\n"))
rt.log("slotb finished", #lines)
