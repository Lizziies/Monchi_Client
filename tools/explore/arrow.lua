rt.run("lib")
local hits = rt.heap(0x66617263656e696d, 400000)
local lines = {"minecraf hits " .. #hits}
for _, a in ipairs(hits) do
    if rt.u64(a + 8) == 0x00776f7272613a74 then lines[#lines + 1] = string.format("str %x  len %s cap %s", a, tostring(rt.u64(a + 16)), tostring(rt.u64(a + 24))) end
end
rt.out("arrow.txt", table.concat(lines, "\n"))
rt.log("arrow fin", #lines)
