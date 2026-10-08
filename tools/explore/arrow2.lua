rt.run("lib")
local hits = rt.heap(0x66617263656e696d, 400000)
local str
for _, a in ipairs(hits) do
    if rt.u64(a + 8) == 0x00776f7272613a74 and rt.u64(a + 16) == 15 and rt.u64(a + 24) == 15 then str = a break end
end
if not str then rt.log("arrow2 none") return end
local lines = {string.format("str %x", str)}
local refs = rt.heapr(str - 0x1000, str + 0x20, 300)
for _, r in ipairs(refs) do
    local v = rt.u64(r)
    lines[#lines + 1] = string.format("at %x -> %x (str-0x%x)", r, v, str - v)
end
rt.out("arrow2.txt", table.concat(lines, "\n"))
rt.log("arrow2 fin", #lines)
