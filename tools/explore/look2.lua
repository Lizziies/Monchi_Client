rt.run("lib")
local lines = {}
for _, a in ipairs({0x1c8e3975c30, 0x1c8e7097858, 0x1c8e7097864, 0x1c8e7097e28, 0x1c8e3593828, 0x1c8e6f3cd08, 0x1c8e6f3cf38, 0x1c8b1b2aab8, 0x1ca19c2e6c8, 0x1ca181f6b60, 0x1ca1acbbf6c}) do
    local f = {}
    for k = -2, 10 do f[#f + 1] = string.format("%.3f", rt.f32(a + 4 * k) or 0) end
    lines[#lines + 1] = string.format("%x : %s", a, table.concat(f, " "))
end
rt.out("look2.txt", table.concat(lines, "\n"))
rt.log("look2 fin")
