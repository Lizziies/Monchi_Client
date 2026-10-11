rt.run("lib")
local str = 0x2c7fc3a65a0
local node = str - 0x60
local lines = {}
for k = 0, 0xb8, 8 do
    local q = rt.u64(node + k) or 0
    local extra = ""
    if q > 0x10000 and q < 0x7ff000000000 then
        local q0 = rt.u64(q)
        extra = string.format("  -> [%x]", q0 or 0)
        if q0 and q0 >= BASE and q0 < BASE + 0x14000000 then extra = extra .. string.format(" vtable rva %x", q0 - BASE) end
    end
    lines[#lines + 1] = string.format("+0x%02x %016x%s", k, q, extra)
end
rt.out("node.txt", table.concat(lines, "\n"))
rt.log("node fin")
