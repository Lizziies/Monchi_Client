rt.run("lib")
rt.run("flagtag")
-- bytes that hold one value in every sample of phase A and C and another value in every sample of phase B
local player = rt.u64(BASE + 0x11d61a70)
local nodes, seen = {{addr = player, path = "player", depth = 0}}, {[player] = true}
local head = 1
while head <= #nodes and #nodes < (MAXN or 3000) do
    local n = nodes[head]; head = head + 1
    if n.depth < (DEPTH or 2) then
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
local phases = {}
for p = 1, 3 do
    phases[p] = {}
    rt.log("flags2 phase", p)
    if p > 1 then rt.sleep(1500) end
    for k = 1, 4 do phases[p][k] = snap(); rt.sleep(400) end
    rt.sleep(1000)
end
local lines = {}
for i, n in ipairs(nodes) do
    local len = 1 << 30
    for p = 1, 3 do for k = 1, 4 do len = math.min(len, #phases[p][k][i]) end end
    for off = 1, len do
        local a = phases[1][1][i]:byte(off)
        local ok = true
        for k = 1, 4 do if phases[1][k][i]:byte(off) ~= a or phases[3][k][i]:byte(off) ~= a then ok = false break end end
        if ok then
            local b = phases[2][1][i]:byte(off)
            if b ~= a then
                for k = 2, 4 do if phases[2][k][i]:byte(off) ~= b then ok = false break end end
                if ok then lines[#lines + 1] = string.format("%s +0x%x  %02x -> %02x", n.path, off - 1, a, b) end
            end
        end
    end
end
rt.out("flags2_" .. FLAGTAG .. ".txt", table.concat(lines, "\n"))
rt.log("flags2 done", #lines)
