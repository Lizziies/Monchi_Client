rt.run("lib")
local str = 0x224848d63c0
local refs = rt.heapr(str - 0x800, str + 0x40, 200)
local lines = {}
for _, r in ipairs(refs) do
    local v = rt.u64(r)
    lines[#lines + 1] = string.format("at %x -> %x (str%+d)", r, v, v - str)
end
rt.out("item3.txt", table.concat(lines, "\n"))
rt.log("item3 fin", #lines)
