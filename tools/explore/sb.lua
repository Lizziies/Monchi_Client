rt.run("lib")
local out = {}
local hits = rt.heap(0x6a626f6968636f6d, 50)
for _, a in ipairs(hits) do
    if rt.u64(a + 16) == 8 then
        local near = {}
        for k = -0x80, 0x80, 8 do
            local l, c = rt.u64(a + k + 16) or 0, rt.u64(a + k + 24) or 0
            if k ~= 0 and l > 0 and l < 40 and c >= l and c < 0x100 then
                local s = rt.cstr(c > 15 and rt.u64(a + k) or (a + k), l)
                if s and s:match("^[%w_ ]+$") then near[#near + 1] = string.format("%d:'%s'", k, s) end
            end
        end
        out[#out + 1] = string.format("%x %s", a, table.concat(near, " "))
        local refs = rt.heapr(a - 0x100, a, 12)
        for _, r in ipairs(refs) do out[#out + 1] = string.format("    ref %x -> obj+%x", r, a - rt.u64(r)) end
    end
end
rt.out("sb.txt", table.concat(out, "\n"))
rt.log("sb fin")
