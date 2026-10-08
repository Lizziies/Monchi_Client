rt.run("lib")
local cis = rt.heap(BASE + 0x0e9731b0, 8)
local player = rt.u64(BASE + 0x11d61a70)
local s = {}
for _, c in ipairs(cis) do
    s[#s + 1] = string.format("%x", c)
    for off = 0, 0x2000, 8 do if rt.u64(c + off) == player then s[#s + 1] = string.format("(player at ci+0x%x)", off) end end
end
for off = 0, 0x800, 8 do local v = rt.u64(player + off); for _, c in ipairs(cis) do if v == c then s[#s + 1] = string.format("[ci at player+0x%x]", off) end end end
rt.log("ci", #cis, table.concat(s, " "))
