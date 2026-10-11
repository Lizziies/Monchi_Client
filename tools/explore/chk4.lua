rt.run("lib")
local out = {}
for _, ch in ipairs({0x290e15cf378, 0x29347225c08}) do
    out[#out + 1] = string.format("== chest %x", ch)
    for k = -0x100, 0x8, 8 do
        local v = rt.u64(ch + k) or 0
        local note = (v >= BASE and v < BASE + 0x14000000) and string.format(" rva %x", v - BASE) or ""
        out[#out + 1] = string.format("  %d %x%s", k, v, note)
    end
    for _, r in ipairs(rt.heapr(ch - 0x200, ch, 10)) do out[#out + 1] = string.format("  ref at %x -> %x (chest-0x%x)", r, rt.u64(r), ch - rt.u64(r)) end
end
rt.out("chk4.txt", table.concat(out, "\n"))
rt.log("chk4 fin")
