rt.run("lib")
rt.run("vttag")
-- inventory arrays: 36 ItemStacks 0x98 apart; then the vector holding one and the path from the player to it
local player = rt.u64(BASE + 0x11d61a70)
local vt = 0
for _, cand in ipairs(rt.find("48 8D 05 ? ? ? ? 48 89 01 48 8B 42 08", 4) or {}) do end
local lines = {}
local stacks = {}
for _, s in ipairs(rt.heap(VT, 20000)) do stacks[s] = true end
local arrays = {}
for s in pairs(stacks) do
    if not stacks[s - 0x98] then
        local n = 0
        while stacks[s + n * 0x98] do n = n + 1 end
        if n == 36 then arrays[#arrays + 1] = s end
    end
end
lines[#lines + 1] = "arrays " .. #arrays
local holders = {}
for _, arr in ipairs(arrays) do
    for _, r in ipairs(rt.heap(arr, 20)) do
        if rt.u64(r + 8) == arr + 36 * 0x98 then holders[#holders + 1] = r; lines[#lines + 1] = string.format("array %x vector %x", arr, r) end
    end
end
local nodes, seen = {{addr = player, path = "player", depth = 0}}, {[player] = true}
local head = 1
while head <= #nodes and #nodes < 80000 do
    local n = nodes[head]; head = head + 1
    for _, t in ipairs(holders) do
        if t >= n.addr and t < n.addr + 0x1000 then lines[#lines + 1] = string.format("PATH %s +0x%x", n.path, t - n.addr) end
    end
    if n.depth < 4 then
        for _, p in ipairs(rt.pointers(n.addr, n.depth == 0 and 0x2000 or 0x1000)) do
            if not seen[p[2]] then seen[p[2]] = true; nodes[#nodes + 1] = {addr = p[2], path = string.format("%s>0x%x", n.path, p[1]), depth = n.depth + 1} end
        end
    end
end
rt.out("findinv.txt", table.concat(lines, "\n"))
rt.log("findinv fin")
