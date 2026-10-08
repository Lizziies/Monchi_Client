rt.run("lib")
local player = rt.u64(BASE + 0x11d61a70)
local b = rt.u64(rt.u64(rt.u64(player + 0x350) + 0x378) + 0x198)
local st = b + 0x98
local out = {"stack " .. (rt.hex(st, 0x98) or "?")}
for k = 0, 0x90, 8 do
    local p = rt.u64(st + k)
    if p and p > 0x10000 and p < 0x7fffffffffff then
        local h = rt.hex(p, 0x60)
        if h then out[#out + 1] = string.format("+%x -> %x : %s", k, p, h) end
    end
end
rt.out("nbt.txt", table.concat(out, "\n"))
rt.log("nbt fin")
