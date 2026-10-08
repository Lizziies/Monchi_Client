rt.run("lib")
local lines = {}
for _, arr in ipairs({0x1e4679d3aa0, 0x1e4cae39dc0, 0x1e4cae3c9e0, 0x1e4cae44e40, 0x1e4cae3b3c0}) do
    for _, r in ipairs(rt.heap(arr, 40)) do
        local e = rt.u64(r + 8)
        if e == arr + 36 * 0x98 then
            lines[#lines + 1] = string.format("array %x  vector at %x  cap %x", arr, r, rt.u64(r + 16) or 0)
        end
    end
end
rt.out("vec.txt", table.concat(lines, "\n"))
rt.log("vec fin", #lines)
