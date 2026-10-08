rt.run("lib")
-- hit results (start = eye position, type 0..3) reachable from the ClientInstance, with their paths
local player = rt.u64(BASE + 0x11d61a70)
local sv = rt.u64(player + 0x1e8)
local ex, ey, ez = rt.f32(sv), rt.f32(sv + 4), rt.f32(sv + 8)
local cis = rt.heap(BASE + 0x0e9731b0, 8)
local lines = {string.format("eye %.3f %.3f %.3f  ci count %d", ex, ey, ez, #cis)}
for _, ci in ipairs(cis) do
    local nodes, seen = {{addr = ci, path = "ci", depth = 0}}, {[ci] = true}
    local head = 1
    while head <= #nodes and #nodes < 8000 do
        local n = nodes[head]; head = head + 1
        local span = n.depth == 0 and 0x2000 or 0x1000
        for _, off in ipairs(rt.scanf(n.addr, span, 0.01, ex, ey, ez)) do
            local t = rt.i32(n.addr + off + 0x18)
            if t and t >= 0 and t <= 3 then
                lines[#lines + 1] = string.format("%s +0x%x  type %d  face %d  hit %.2f %.2f %.2f", n.path, off, t, rt.i32(n.addr + off + 0x1c) or -1,
                    rt.f32(n.addr + off + 0x2c) or 0, rt.f32(n.addr + off + 0x30) or 0, rt.f32(n.addr + off + 0x34) or 0)
            end
        end
        if n.depth < 2 then
            for _, p in ipairs(rt.pointers(n.addr, span)) do
                if not seen[p[2]] then
                    seen[p[2]] = true
                    nodes[#nodes + 1] = {addr = p[2], path = string.format("%s>0x%x", n.path, p[1]), depth = n.depth + 1}
                end
            end
        end
    end
end
rt.out("hitci.txt", table.concat(lines, "\n"))
rt.log("hitci done", #lines)
