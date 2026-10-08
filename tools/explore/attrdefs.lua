rt.run("lib")
local out = {}
for rva = 0x11dc5000, 0x11dc5c00, 8 do
    local p = BASE + rva + 0x10
    local len = rt.u64(p + 16) or 0
    if len > 5 and len < 64 then
        local cap = rt.u64(p + 24) or 0
        local s = rt.cstr(cap > 15 and rt.u64(p) or p, len)
        if s and s:find("^minecraft:") then out[#out + 1] = string.format("rva %x %s", rva, s) end
    end
end
-- who references the hunger def (instances)
rt.out("attrdefs.txt", table.concat(out, "\n"))
rt.log("attrdefs fin")
