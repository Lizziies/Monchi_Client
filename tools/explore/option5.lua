rt.run("lib")
local tag, opts = 0x28b8f951838, 0x28c3f2a8200
local lines = {}
for k = 0, 0x1800, 8 do
    local p = rt.u64(opts + k) or 0
    local d = tag - p
    if d >= 0 and d < 0x400 then lines[#lines + 1] = string.format("opts+0x%x -> %x  (tag at +0x%x)", k, p, d) end
end
rt.out("option5.txt", table.concat(lines, "\n"))
rt.log("option5 done")
