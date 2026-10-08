rt.run("lib")
local player = rt.u64(BASE + 0x11d61a70)
local function fl(a, n) local s = {} for k = 0, n - 1 do s[#s + 1] = string.format("%.3f", rt.f32(a + k * 4) or 0) end return table.concat(s, " ") end
local lines = {}
rt.log("live start")
for n = 1, 30 do
    local sv = rt.u64(player + 0x1e8)
    local cam = rt.u64(rt.u64(player + 0x138) + 0x990)
    lines[#lines + 1] = string.format("%2d sv %s | sv88 %s | cam %s", n, fl(sv, 12), fl(sv + 0x88, 6), fl(cam, 10))
    rt.sleep(100)
end
rt.out("live.txt", table.concat(lines, "\n"))
rt.log("live done")
