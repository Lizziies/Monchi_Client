rt.run("lib")
-- view angles one pointer away from the player: A, turned right (yaw), B, turned down (pitch), C
local player = rt.u64(BASE + 0x11d61a70)
local nodes = {{addr = player, path = "player", span = 0x1800}}
for _, p in ipairs(rt.pointers(player, 0x1800)) do nodes[#nodes + 1] = {addr = p[2], path = string.format("player>0x%x", p[1]), span = 0x1000} end
local function snap()
    local s = {}
    for i, n in ipairs(nodes) do s[i] = rt.raw(n.addr, n.span) or "" end
    return s
end
local a = snap(); rt.log("rot1 A"); rt.sleep(3000)
local b = snap(); rt.log("rot1 B"); rt.sleep(3000)
local c = snap()
local lines = {}
for i, n in ipairs(nodes) do
    local len = math.min(#a[i], #b[i], #c[i])
    for off = 1, len - 3, 4 do
        local va, vb, vc = string.unpack("<f", a[i], off), string.unpack("<f", b[i], off), string.unpack("<f", c[i], off)
        if va == va and vb == vb and vc == vc and math.abs(va) <= 360 and math.abs(vb) <= 360 and math.abs(vc) <= 360 then
            if math.abs(vb - va) > 5 and math.abs(vc - vb) < 0.01 then lines[#lines + 1] = string.format("yaw?   %s +0x%x  %.2f %.2f %.2f", n.path, off - 1, va, vb, vc) end
            if math.abs(vb - va) < 0.01 and math.abs(vc - vb) > 5 then lines[#lines + 1] = string.format("pitch? %s +0x%x  %.2f %.2f %.2f", n.path, off - 1, va, vb, vc) end
        end
    end
end
table.sort(lines)
rt.out("rot1.txt", table.concat(lines, "\n"))
rt.log("rot1 done", #lines)
