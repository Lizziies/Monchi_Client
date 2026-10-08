rt.run("lib")
local page = rt.u64(rt.u64(0x208cf604e10 + 0x50))
local out = {}
local function str(at)
    local len, cap = rt.u64(at + 16) or 0, rt.u64(at + 24) or 0
    if len < 1 or len > 80 or cap < len or cap > 0x1000 then return nil end
    return rt.cstr(cap > 15 and rt.u64(at) or at, len)
end
for k = 0, 0x1c0, 8 do
    local s = str(page + k)
    out[#out + 1] = string.format("+%03x %016x %s", k, rt.u64(page + k) or 0, s and ("'" .. s .. "'") or "")
end
rt.out("pool9.txt", table.concat(out, "\n"))
rt.log("pool9 fin")
