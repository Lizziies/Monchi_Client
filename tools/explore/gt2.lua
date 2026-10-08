rt.run("lib")
local p = rt.u64(BASE + 0x11d61a70)
local ld = rt.u64(p + 0x90)
local out = {string.format("ld %x first %x", ld, rt.u64(ld) or 0)}
for _, r in ipairs(rt.heap(ld, 30)) do
    -- find object start: nearest preceding qword that is an image vtable within 0x2000
    for back = 0, 0x2000, 8 do
        local v = rt.u64(r - back)
        if v and v >= BASE and v < BASE + 0x14000000 then
            local f = rt.u64(v + 0x420)
            local d = (f and f >= BASE and f < BASE + 0x14000000) and rt.disasm(f, 5):gsub("\n", " | ") or "-"
            out[#out + 1] = string.format("ref at %x obj %x (+0x%x) vt rva %x slot420: %s", r, r - back, back, v - BASE, d)
            break
        end
    end
end
rt.out("gt2.txt", table.concat(out, "\n"))
rt.log("gt2 fin")
