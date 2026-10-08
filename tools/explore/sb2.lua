rt.run("lib")
local out = {}
local function str(a)
    local l, c = rt.u64(a + 16) or 0, rt.u64(a + 24) or 0
    if l == 0 or l > 60 or c < l or c > 0x1000 then return nil end
    return rt.cstr(c > 15 and rt.u64(a) or a, l)
end
local pl = rt.u64(BASE + 0x11d61a70)
local level = rt.u64(pl + 0x90)
out[#out + 1] = string.format("level %x", level)
for _, r in ipairs({0x207914e7c10, 0x2079c65f5f0, 0x207a2e72708, 0x207a2e72f50, 0x208d4373a30, 0x20790d64160, 0x20790d66658, 0x208cca8e7b0}) do
    local node = r - 0x30
    local line = string.format("ref %x key '%s':", r, tostring(str(node + 0x10)))
    local n, head = node, nil
    for i = 1, 40 do
        n = rt.u64(n)
        if not n or n == node then break end
        local k = str(n + 0x10)
        if not k then head = n end
        line = line .. " " .. tostring(k)
    end
    out[#out + 1] = line
    if head then
        local refs = rt.heap(head, 8)
        for _, h in ipairs(refs) do out[#out + 1] = string.format("    head %x held at %x (level+%x)", head, h, h - level) end
    end
end
rt.out("sb2.txt", table.concat(out, "\n"))
rt.log("sb2 fin")
