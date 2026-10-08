rt.run("lib")
-- "mochipro" little endian
local hits = rt.heap(0x6f72706968636f6d, 200)
local out = {}
for _, a in ipairs(hits) do
    local s = rt.cstr(a, 40) or ""
    local refs = {}
    for back = 0, 0x40, 1 do
        local start = a - back
        if back == 0 or rt.u8(start) ~= 0 then end
    end
    local r = rt.heapr(a, a + 1, 8)
    local rs = {}
    for _, x in ipairs(r) do rs[#rs + 1] = string.format("%x", x) end
    out[#out + 1] = string.format("%x '%s' refs: %s  before: %s", a, s, table.concat(rs, " "), rt.cstr(a - 16, 16) or "")
end
rt.out("chat.txt", table.concat(out, "\n"))
rt.log("chat fin", #hits)
