rt.run("lib")
local a = 0x210c5609674
local base = a - (a % 8)
local out = {}
for k = -0xa0, 0x20, 8 do
    local q = rt.u64(base + k) or 0
    local note = ""
    if q >= BASE and q < BASE + 0x14000000 then note = string.format(" rva %x", q - BASE)
    elseif q > 0x10000 and q < 0x7fffffffffff then
        local s = rt.cstr(q, 40)
        if s and #s > 3 and s:match("^[%w_:%.]+$") then note = " str " .. s end
        local q2 = rt.u64(q)
        if q2 and q2 >= BASE and q2 < BASE + 0x14000000 then note = note .. string.format(" ->rva %x", q2 - BASE) end
        local s2 = q2 and rt.cstr(q2, 40)
    end
    out[#out + 1] = string.format("%5d %016x f=%.2f,%.2f%s", k, q, rt.f32(base + k) or 0, rt.f32(base + k + 4) or 0, note)
end
rt.out("hp2.txt", table.concat(out, "\n"))
rt.log("hp2 fin")
