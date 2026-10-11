rt.run("lib")
-- 32-bit counters near the player that grow by about 20 per second (game ticks, time of day)
local player = rt.u64(BASE + 0x11d61a70)
local nodes, seen = {{addr = player, path = "player", depth = 0}}, {[player] = true}
local head = 1
while head <= #nodes and #nodes < 5000 do
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
local a = snap()
rt.sleep(2000)
local b = snap()
rt.sleep(2000)
local c = snap()
local lines = {}
for i, n in ipairs(nodes) do
    local sa, sb, sc = a[i], b[i], c[i]
    local len = math.min(#sa, #sb, #sc)
    for off = 1, len - 3, 4 do
        local va, vb, vc = string.unpack("<i4", sa, off), string.unpack("<i4", sb, off), string.unpack("<i4", sc, off)
        local d1, d2 = vb - va, vc - vb
        if d1 >= 30 and d1 <= 50 and d2 >= 30 and d2 <= 50 then
            lines[#lines + 1] = string.format("%s +0x%x  %d -> %d -> %d", n.path, off - 1, va, vb, vc)
        end
    end
end
rt.out("ticks.txt", table.concat(lines, "\n"))
rt.log("ticks done", #lines)
