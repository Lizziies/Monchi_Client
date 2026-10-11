rt.run("lib")

-- angles near the player: snapshot, wait while the camera is turned up, snapshot, wait while it is turned right, snapshot
local player = rt.u64(BASE + 0x11d61a70)
local nodes, seen = {{addr = player, path = "player"}}, {[player] = true}
local head = 1
while head <= #nodes and #nodes < 4000 do
    local n = nodes[head]
    head = head + 1
    local depth = n.depth or 0
    if depth < 2 then
        for _, p in ipairs(rt.pointers(n.addr, depth == 0 and 0x2000 or 0x1000)) do
            if not seen[p[2]] then
                seen[p[2]] = true
                nodes[#nodes + 1] = {addr = p[2], path = string.format("%s>0x%x", n.path, p[1]), depth = depth + 1}
            end
        end
    end
end

local function snap()
    local s = {}
    for _, n in ipairs(nodes) do
        local list = rt.floatsin(n.addr, (n.depth or 0) == 0 and 0x2000 or 0x1000, -360, 360)
        for i = 1, #list, 2 do s[n.addr + list[i]] = list[i + 1] end
    end
    return s
end

local a = snap()
rt.log("rot A done")
rt.sleep(5000)
local b = snap()
rt.log("rot B done")
rt.sleep(5000)
local c = snap()

local where = {}
for _, n in ipairs(nodes) do where[n.addr] = n end
local lines = {}
for addr, va in pairs(a) do
    local vb, vc = b[addr], c[addr]
    if vb and vc and math.abs(vb - va) > 2 and math.abs(vc - vb) < 0.5 then
        lines[#lines + 1] = string.format("pitch? %x  %.3f -> %.3f -> %.3f", addr, va, vb, vc)
    elseif vb and vc and math.abs(vb - va) < 0.5 and math.abs(vc - vb) > 2 then
        lines[#lines + 1] = string.format("yaw?   %x  %.3f -> %.3f -> %.3f", addr, va, vb, vc)
    end
end
-- map addresses back to paths
for i, l in ipairs(lines) do
    local addr = tonumber(l:match("%s(%x+)%s"), 16)
    for _, n in ipairs(nodes) do
        local span = (n.depth or 0) == 0 and 0x2000 or 0x1000
        if addr >= n.addr and addr < n.addr + span then
            lines[i] = l .. string.format("   %s +0x%x", n.path, addr - n.addr)
            break
        end
    end
end
table.sort(lines)
rt.out("rot.txt", table.concat(lines, "\n"))
rt.log("rot done", #lines)
