rt.run("lib")
local pl = rt.u64(BASE + 0x11d61a70)
local objs = {rt.u64(pl + 0x130), rt.u64(pl + 0x5b0)}
local series = {}
for t = 1, 70 do
    for i, o in ipairs(objs) do
        for k = 0, 0x100, 4 do
            local key = string.format("%d+%x", i, k)
            local v = rt.f32(o + k)
            series[key] = (series[key] or "") .. string.format(" %.2f", v or 0)
        end
    end
    rt.sleep(60)
end
local out = {}
for key, s in pairs(series) do
    local first = s:match("^ (%S+)")
    local changed = false
    for v in s:gmatch("%S+") do if v ~= first then changed = true end end
    if changed then out[#out + 1] = key .. ":" .. s end
end
table.sort(out)
rt.out("gm2.txt", table.concat(out, "\n"))
rt.log("gm2 fin", #out)

