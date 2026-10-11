rt.run("lib")
local out = {}
for _, a in ipairs({0x1e4b4d85732, 0x1e4b4d857ca, 0x1e4cae3b3e2, 0x1e4cae3ca02, 0x1e4cae44e62}) do
    out[#out + 1] = string.format("== %x val %d", a, rt.u8(a))
    local base = a - (a % 8) - 0x30
    for k = 0, 0x58, 8 do
        local q = rt.u64(base + k) or 0
        local note = ""
        if q > 0x10000 and q < 0x7fffffffffff then
            local s = rt.cstr(q, 40)
            local v = rt.u64(q)
            if v and v >= BASE and v < BASE + 0x14000000 then note = string.format(" vt rva %x", v - BASE) end
            if s and #s > 3 and s:match("^[%w_:%.]+$") then note = note .. " str " .. s end
        end
        out[#out + 1] = string.format("  %x %016x%s", base + k, q, note)
    end
end
rt.out("cv.txt", table.concat(out, "\n"))
rt.log("cv fin")
