rt.run("lib")
local s = {}
for k = 1, 16 do
    local t = {}
    for _, b in ipairs({0x196da6ea3c0, 0x1983b5a72a0, 0x196d9b2dda0, 0x1984f9a30a0}) do t[#t + 1] = string.format("%.2f", rt.f32(b + 0x10) - rt.f32(b + 4)) end
    s[#s + 1] = table.concat(t, "/")
    rt.sleep(250)
end
rt.log("mv2 fin", table.concat(s, " "))
