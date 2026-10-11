rt.run("lib")
local opts, opt = 0x28c3f2a8200, 0x28b8f951820
local lines = {}
local function dumpq(name, a, n)
    lines[#lines + 1] = name .. string.format(" %x", a)
    for k = 0, n - 8, 8 do lines[#lines + 1] = string.format("  +0x%02x  %016x  %.4f %.4f", k, rt.u64(a + k) or 0, rt.f32(a + k) or 0, rt.f32(a + k + 4) or 0) end
end
dumpq("option", opt, 0x40)
for _, pat in ipairs({"48 8B ?? A0 01 00 00 F3 0F 10 ?? 18", "48 8B ?? A0 01 00 00 F3 ?? 0F 10 ?? 18", "48 8B ?? A0 01 00 00 48 8B ?? F3 0F 10 ?? 18"}) do
    for _, h in ipairs(rt.find(pat, 64)) do
        local f = rt.func(h)
        lines[#lines + 1] = string.format("code %x  func %s  (%s)", h, f and string.format("%x +0x%x", f, h - f) or "?", pat)
    end
end
local player = rt.u64(BASE + 0x11d61a70)
local seen, q, head = {[player] = true}, {{a = player, p = "player", d = 0}}, 1
while head <= #q and #q < 20000 do
    local n = q[head]; head = head + 1
    for _, pr in ipairs(rt.pointers(n.a, n.d == 0 and 0x2000 or 0x1000)) do
        if pr[2] == opts then lines[#lines + 1] = string.format("PATH %s>0x%x", n.p, pr[1]) end
        if n.d < 3 and not seen[pr[2]] then
            seen[pr[2]] = true
            q[#q + 1] = {a = pr[2], p = string.format("%s>0x%x", n.p, pr[1]), d = n.d + 1}
        end
    end
end
rt.out("gammafn.txt", table.concat(lines, "\n"))
rt.log("gammafn done")
