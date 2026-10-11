rt.run("lib")
rt.run("paths_target")

-- breadth-first walk over pointers starting at the local player, looking for TARGET floats (see paths_target.lua)
local ciVtable = BASE + 0x0e9731b0
local getLocalPlayer = rt.vtable(ciVtable, 600)[0x540 // 8 + 1]
local player, ci
for _, inst in ipairs(rt.heap(ciVtable, 8)) do
    local ok, lp = pcall(rt.call, getLocalPlayer, inst)
    if ok and lp ~= 0 and lp > 0x10000 then player, ci = lp, inst break end
end
if not player then rt.log("paths: no player") return end

local report = {string.format("ci %x player %x target %s", ci, player, table.concat(TARGET, " "))}
local seen = {[player] = true}
local queue = {{addr = player, path = "player"}}
local head, nodes = 1, 0
while head <= #queue and nodes < (MAXNODES or 6000) do
    local n = queue[head]
    head = head + 1
    nodes = nodes + 1
    local span = n.depth == nil and (ROOTSPAN or 0x1800) or (SPAN or 0x600)
    for _, off in ipairs(rt.scanf(n.addr, span, TOL or 0.01, table.unpack(TARGET))) do
        report[#report + 1] = string.format("HIT %s +0x%x  (%x)", n.path, off, n.addr + off)
    end
    local depth = n.depth or 0
    if depth < (MAXDEPTH or 3) then
        for _, p in ipairs(rt.pointers(n.addr, span)) do
            local off, v = p[1], p[2]
            if not seen[v] then
                seen[v] = true
                queue[#queue + 1] = {addr = v, path = string.format("%s>0x%x", n.path, off), depth = depth + 1}
            end
        end
    end
end
report[#report + 1] = string.format("nodes %d queued %d", nodes, #queue)
rt.out("paths_" .. (TAG or "x") .. ".txt", table.concat(report, "\n"))
rt.log("paths done", TAG or "x", #report - 2)
