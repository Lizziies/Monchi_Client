rt.run("lib")
local player = rt.u64(BASE + 0x11d61a70)
local nodes, seen = {{addr = player, path = "player", depth = 0}}, {[player] = true}
local head = 1
while head <= #nodes and #nodes < 5000 do
    local n = nodes[head]; head = head + 1
    if n.depth < 2 then
        for _, p in ipairs(rt.pointers(n.addr, n.depth == 0 and 0x1800 or 0x800)) do
            if not seen[p[2]] then
                seen[p[2]] = true
                nodes[#nodes + 1] = {addr = p[2], path = string.format("%s>0x%x", n.path, p[1]), depth = n.depth + 1}
            end
        end
    end
end
local function snap()
    local s = {}
    for i, n in ipairs(nodes) do s[i] = rt.raw(n.addr, n.depth == 0 and 0x1800 or 0x800) or "" end
    return s
end
local a = snap()
rt.log("cnt snapA", #nodes)
rt.sleep(4000)
local b = snap()
local lines = {}
for i, n in ipairs(nodes) do
    local sa, sb = a[i], b[i]
    for off = 1, math.min(#sa, #sb) do
        if sa:byte(off) == 37 and sb:byte(off) == 36 then lines[#lines + 1] = string.format("%s +0x%x  (%x)", n.path, off - 1, n.addr + off - 1) end
    end
end
rt.out("count.txt", table.concat(lines, "\n"))
rt.log("cnt finished", #lines)
