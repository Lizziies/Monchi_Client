rt.run("lib")
local pl = rt.u64(BASE + 0x11d61a70)
local out = {}
for o = 0, 0x2000, 8 do
    local p = rt.u64(pl + o)
    if p and p > 0x10000000 and p < 0x7fffffffffff and p ~= pl then
        local vt = rt.u64(p)
        if vt and vt > BASE and vt < BASE + 0x20000000 then
            for k = 8, 0x20, 8 do
                if rt.u64(p + k) == pl then out[#out + 1] = string.format("player+%x -> %x vt %x back at +%x", o, p, vt - BASE, k) end
            end
        end
    end
end
rt.out("gm.txt", table.concat(out, "\n"))
rt.log("gm fin", #out)
