rt.run("lib")
-- floats that read exactly +90 while looking straight down and -90 straight up, within two pointers of the player
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
local function find(v)
    local s = {}
    for _, n in ipairs(nodes) do
        for _, off in ipairs(rt.scanf(n.addr, n.depth == 0 and 0x1800 or 0x1000, 0.05, v)) do s[n.path .. string.format(" +0x%x", off)] = n.addr + off end
    end
    return s
end
local down = find(89.9)
rt.log("pitch down done")
rt.sleep(3000)
local up = find(-89.9)
local lines = {}
for k, addr in pairs(down) do
    if up[k] then lines[#lines + 1] = string.format("%s  next float %.2f  prev %.2f", k, rt.f32(addr + 4), rt.f32(addr - 4)) end
end
table.sort(lines)
rt.out("pitch.txt", table.concat(lines, "\n"))
rt.log("pitch done", #lines)
