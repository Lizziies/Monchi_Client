rt.run("lib")
local p = rt.u64(BASE + 0x11d61a70)
local a = rt.u64(p + 0x38)
local lv = a and rt.u64(a + 0x1d8)
local vt = lv and rt.u64(lv)
local f = vt and rt.u64(vt + 0x420)
local out = {string.format("p %x a %x lv %x vt %x f %x", p, a or 0, lv or 0, vt or 0, f or 0)}
if f then out[#out + 1] = rt.disasm(f, 14); out[#out + 1] = "hex " .. rt.hex(f, 40) end
rt.out("gt.txt", table.concat(out, "\n"))
rt.log("gt fin")
