rt.run("lib")
rt.run("c3tag")
local player = rt.u64(BASE + 0x11d61a70)
local nodes, seen = {{addr = player, path = "player", depth = 0}}, {[player] = true}
local head = 1
while head <= #nodes and #nodes < 6000 do
    local n = nodes[head]; head = head + 1
    if n.depth < 2 then
        for _, p in ipairs(rt.pointers(n.addr, n.depth == 0 and 0x1800 or 0x1000)) do
            if not seen[p[2]] then
                seen[p[2]] = true
                nodes[#nodes + 1] = {addr = p[2], path = string.format("%s>0x%x", n.path, p[1]), depth = n.depth + 1}
            end
        end
    end
end
local function snap()
    local s = {}
    for i, n in ipairs(nodes) do s[i] = rt.raw(n.addr, n.depth == 0 and 0x1800 or 0x1000) or "" end
    return s
end
local a = snap(); rt.log("c3 phaseA", A0); rt.sleep(6000)
local b = snap(); rt.log("c3 phaseB"); rt.sleep(6000)
local c = snap()
local lines = {}
for i, n in ipairs(nodes) do
    local sa, sb, sc = a[i], b[i], c[i]
    for off = 1, math.min(#sa, #sb, #sc) do
        if sa:byte(off) == A0 and sb:byte(off) == A0 - 1 and sc:byte(off) == A0 - 2 then lines[#lines + 1] = string.format("%s +0x%x  (%x)", n.path, off - 1, n.addr + off - 1) end
    end
end
rt.out("count3.txt", table.concat(lines, "\n"))
rt.log("c3 finished", #lines)
