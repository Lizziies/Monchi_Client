rt.run("lib")
local player = rt.u64(BASE + 0x11d61a70)
local s = {}
for n = 1, 60 do
    s[#s + 1] = tostring(rt.u8(rt.u64(player + 0x1a0) + 0xc))
    if n % 20 == 0 then rt.log("mstate", n) end
    rt.sleep(150)
end
rt.out("mstate.txt", table.concat(s, " "))
rt.log("mstate done")
