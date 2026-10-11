rt.run("lib")
local lines = {}
local player = rt.u64(BASE + 0x11d61a70)
lines[#lines + 1] = string.format("player %x", player)
for _, t in ipairs({0x1e4679d3aa0, 0x1e4b5c4db80, 0x1e4cae39dc0}) do
    for _, r in ipairs(rt.heapr(t - 0x1000, t + 1, 40)) do
        local v = rt.u64(r)
        lines[#lines + 1] = string.format("%x <- at %x  value %x (t-0x%x)", t, r, v, t - v)
    end
end
rt.out("invref.txt", table.concat(lines, "\n"))
rt.log("invref fin")
