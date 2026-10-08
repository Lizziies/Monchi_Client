rt.run("lib")
local f = rt.func(BASE + 0x466dc56)
local out = {string.format("func %s", f and string.format("%x", f - BASE) or "?")}
out[#out + 1] = rt.disasm(BASE + 0x466dc20, 30)
for _, c in ipairs(rt.callers(f or (BASE + 0x466dc56), 10) or {}) do
    local g = rt.func(c)
    out[#out + 1] = string.format("caller %x func %s", c - BASE, g and string.format("%x", g - BASE) or "?")
end
rt.out("crash.txt", table.concat(out, "\n"))
rt.log("crash fin")
