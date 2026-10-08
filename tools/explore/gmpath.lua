rt.run("lib")
local target = 0x196d83b01b0
local out = {}
local roots = {{rt.u64(BASE + 0x11d61a70), "player"}}
for _, c in ipairs(rt.heap(BASE + 0x0e9731b0, 8)) do roots[#roots + 1] = {c, string.format("ci%x", c)} end
for _, root in ipairs(roots) do
    local nodes, seen = {{addr = root[1], path = root[2], depth = 0}}, {[root[1]] = true}
    local head = 1
    while head <= #nodes and #nodes < 40000 do
        local n = nodes[head]; head = head + 1
        local span = n.depth == 0 and 0x2000 or 0x1000
        if target >= n.addr and target < n.addr + span then out[#out + 1] = string.format("%s +0x%x", n.path, target - n.addr) end
        if n.depth < 3 then
            for _, p in ipairs(rt.pointers(n.addr, span)) do
                if not seen[p[2]] then seen[p[2]] = true; nodes[#nodes + 1] = {addr = p[2], path = string.format("%s>0x%x", n.path, p[1]), depth = n.depth + 1} end
            end
        end
    end
end
rt.out("gmpath.txt", table.concat(out, "\n"))
rt.log("gmpath fin", #out)
