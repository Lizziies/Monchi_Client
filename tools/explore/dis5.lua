rt.run("lib")
local out = {}
for _, rva in ipairs({0x8db97b0, 0xa00d300}) do
    out[#out + 1] = string.format("### %x callers:", rva)
    for _, c in ipairs(rt.callers(BASE + rva, 8) or {}) do local f = rt.func(c); out[#out + 1] = string.format("  call from %x (func %s)", c - BASE, f and string.format("%x", f - BASE) or "?") end
    local refs = rt.heapr and {} or {}
    out[#out + 1] = rt.disfunc(BASE + rva, 70)
end
rt.out("dis5.txt", table.concat(out, "\n"))
rt.log("dis5 fin")
