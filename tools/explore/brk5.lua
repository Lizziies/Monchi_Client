rt.run("lib")
local pl = rt.u64(BASE + 0x11d61a70)
local hr = rt.u64(pl + 0x1e8)
rt.sleep(1000)
local x, y, z = rt.i32(hr + 0x20), rt.i32(hr + 0x24), rt.i32(hr + 0x28)
local hits = rt.heapi(x, x, y, y, z, z, 200)
rt.log("brk5 block", x, y, z, #hits)
local function snap()
    local t = {}
    for _, a in ipairs(hits) do
        for k = -0x40, 0x40, 4 do t[#t + 1] = rt.f32(a + k) or 0 end
    end
    return t
end
local s1 = snap()
rt.sleep(4000)
local s2 = snap()
rt.sleep(4000)
local s3 = snap()
local out = {}
local i = 0
for _, a in ipairs(hits) do
    for k = -0x40, 0x40, 4 do
        i = i + 1
        if s1[i] > 0 and s2[i] > s1[i] and s3[i] > s2[i] and s3[i] <= 1 then
            out[#out + 1] = string.format("%x %+d  %.4f %.4f %.4f", a, k, s1[i], s2[i], s3[i])
        end
    end
end
rt.out("brk5.txt", table.concat(out, "\n"))
rt.log("brk5 fin", #out)
