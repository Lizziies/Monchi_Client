rt.run("lib")
rt.run("flagtag")
-- bytes near the player that change while a key is held and return afterwards (FLAGTAG names the key)
local player = rt.u64(BASE + 0x11d61a70)
local nodes = {{addr = player, path = "player", span = 0x1800}}
local seen = {[player] = true}
for _, p in ipairs(rt.pointers(player, 0x1800)) do
    if not seen[p[2]] and #nodes < 800 then
        seen[p[2]] = true
        nodes[#nodes + 1] = {addr = p[2], path = string.format("player>0x%x", p[1]), span = 0x800}
    end
end
local function snap()
    local s = {}
    for i, n in ipairs(nodes) do s[i] = rt.hex(n.addr, n.span) or "" end
    return s
end
local a = snap()
rt.log("flags A")
rt.sleep(3000)
local b = snap()
rt.log("flags B")
rt.sleep(3000)
local c = snap()
local lines = {}
for i, n in ipairs(nodes) do
    local sa, sb, sc = a[i], b[i], c[i]
    for k = 1, math.min(#sa, #sb, #sc) - 1, 2 do
        local va, vb, vc = sa:sub(k, k + 1), sb:sub(k, k + 1), sc:sub(k, k + 1)
        if va == vc and va ~= vb then
            lines[#lines + 1] = string.format("%s +0x%x  %s -> %s -> %s", n.path, (k - 1) // 2, va, vb, vc)
        end
    end
end
rt.out("flags_" .. (FLAGTAG or "x") .. ".txt", table.concat(lines, "\n"))
rt.log("flags done", #lines)
