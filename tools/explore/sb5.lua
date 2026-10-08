rt.run("lib")
local out = {}
local def = 0x208d3007a30
local h = rt.hex(def, 0x80) or ""
for i = 1, #h, 96 do out[#out + 1] = h:sub(i, i + 95) end
for k = 0, 0x78, 8 do
    local l, c = rt.u64(def + k + 16) or 0, rt.u64(def + k + 24) or 0
    if l > 0 and l < 60 and c >= l and c < 0x1000 then out[#out + 1] = string.format("+%x '%s'", k, tostring(rt.cstr(c > 15 and rt.u64(def + k) or (def + k), l))) end
end
rt.out("sb5.txt", table.concat(out, "\n"))
rt.log("sb5 fin")
