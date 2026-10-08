rt.run("lib")
local player = rt.u64(BASE + 0x11d61a70)
local nodes, seen = {{addr = player, path = "player", depth = 0}}, {[player] = true}
local head = 1
while head <= #nodes and #nodes < 6000 do
    local n = nodes[head]
    head = head + 1
    if n.depth < 2 then
        for _, p in ipairs(rt.pointers(n.addr, n.depth == 0 and 0x1800 or 0x1000)) do
            if not seen[p[2]] then
                seen[p[2]] = true
                nodes[#nodes + 1] = {addr = p[2], path = string.format("%s>0x%x", n.path, p[1]), depth = n.depth + 1}
            end
        end
    end
end
local function span(n) return n.depth == 0 and 0x1800 or 0x1000 end
local a = {}
for i, n in ipairs(nodes) do
    for _, off in ipairs(rt.scanf(n.addr, span(n), 0.001, 20)) do a[#a + 1] = {n, off} end
end
rt.log("health A", #a)
rt.sleep(8000)
local lines = {}
for _, h in ipairs(a) do
    local v = rt.f32(h[1].addr + h[2])
    if v and v > 13 and v < 17 then lines[#lines + 1] = string.format("%s +0x%x  20 -> %.2f", h[1].path, h[2], v) end
end
rt.out("health.txt", table.concat(lines, "\n"))
rt.log("health done", #lines)
