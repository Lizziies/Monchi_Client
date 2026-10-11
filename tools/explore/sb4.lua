rt.run("lib")
local out = {}
local function str(a)
    local l, c = rt.u64(a + 16) or 0, rt.u64(a + 24) or 0
    if l == 0 or l > 60 or c < l or c > 0x1000 then return nil end
    return rt.cstr(c > 15 and rt.u64(a) or a, l)
end
local pl = rt.u64(BASE + 0x11d61a70)
local sb = rt.u64(pl + 0x490)
out[#out + 1] = string.format("sb %x vt %x", sb, rt.u64(sb) - BASE)
local head = rt.u64(sb + 0x18)
local n = rt.u64(head)
while n and n ~= head do
    local obj = rt.u64(n + 0x30)
    out[#out + 1] = string.format("slot '%s' obj %x name '%s' display '%s'", tostring(str(n + 0x10)), obj, tostring(str(obj + 0x58)), tostring(str(obj + 0x78)))
    local h = rt.hex(obj, 0x58) or ""
    for i = 1, #h, 96 do out[#out + 1] = "   " .. h:sub(i, i + 95) end
    -- first pointer-like fields of the objective: try them as list heads
    for k = 0, 0x50, 8 do
        local hd = rt.u64(obj + k)
        if hd and hd > 0x10000000000 and hd < 0x7fffffffffff then
            local m, cnt = rt.u64(hd), 0
            while m and m ~= hd and cnt < 10 do
                out[#out + 1] = string.format("   obj+%x node %x: %s", k, m, (rt.hex(m + 0x10, 0x20) or "?"))
                m = rt.u64(m); cnt = cnt + 1
            end
        end
    end
    n = rt.u64(n)
end
rt.out("sb4.txt", table.concat(out, "\n"))
rt.log("sb4 fin")

