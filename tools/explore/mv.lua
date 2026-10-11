rt.run("lib")
local box = 0x196da6ea3c0
local mov = 0x196e7bd3ca0 + 8 * 0x88
local s = {}
for k = 1, 40 do
    s[#s + 1] = string.format("%.2f/%.3f/%.3f", rt.f32(box + 0x1c), rt.f32(mov + 0x7c), rt.f32(box + 4))
    if k % 10 == 0 then rt.log("mv tick", k) end
    rt.sleep(200)
end
rt.log("mv fin", table.concat(s, " "))
