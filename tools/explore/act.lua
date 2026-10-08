rt.run("lib")
local player = rt.u64(BASE + 0x11d61a70)
local hr = rt.u64(player + 0x1e8)
local er = rt.u64(hr + 0x38)
local tid = rt.u32(hr + 0x48)
local out = {}
for _, h in ipairs(rt.heap(er, 4000)) do
    local id = rt.u32(h + 0x10)
    if id and id ~= 0 and id < 0x10000000 then
        local a = h - 0x320
        local vt = rt.u64(a) or 0
        if vt >= BASE and vt < BASE + 0x14000000 then out[#out + 1] = string.format("actor %x vt %x id %x%s", a, vt - BASE, id, id == tid and "  <== TARGET" or "") end
    end
end
-- references to the target actor from registry pools
rt.out("act.txt", table.concat(out, "\n"))
rt.log("act fin", #out)
