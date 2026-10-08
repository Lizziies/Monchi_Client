rt.run("lib")
local p = rt.u64(BASE + 0x11d61a70)
local out = {}
for _, base in ipairs({p, rt.u64(p + 8), rt.u64(p + 0x28)}) do
    out[#out + 1] = string.format("== %x", base)
    for k = 0, 0x60, 4 do
        local d = rt.u32(base + k) or 0
        out[#out + 1] = string.format("  +%02x %08x", k, d)
    end
end
rt.out("ctx2.txt", table.concat(out, "\n"))
rt.log("ctx2 fin")
