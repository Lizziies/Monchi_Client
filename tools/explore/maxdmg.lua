rt.run("lib")
local player = rt.u64(BASE + 0x11d61a70)
local b = rt.u64(rt.u64(rt.u64(player + 0x350) + 0x378) + 0x198)
local sword = rt.u64(rt.u64(b + 0x98 + 8))
local arrow = rt.u64(rt.u64(b + 8))
local out = {}
for o = 0, 0x400, 2 do
    if rt.u16(sword + o) == 1561 then out[#out + 1] = string.format("+0x%x sword=1561 arrow=%d", o, rt.u16(arrow + o)) end
end
out[#out + 1] = "stack0 hex " .. rt.hex(b, 0x98)
out[#out + 1] = "stack1 hex " .. rt.hex(b + 0x98, 0x98)
rt.out("maxdmg.txt", table.concat(out, "\n"))
rt.log("maxdmg fin")
