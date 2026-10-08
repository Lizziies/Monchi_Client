rt.run("lib")
local player = rt.u64(BASE + 0x11d61a70)
local b = rt.u64(rt.u64(rt.u64(player + 0x350) + 0x378) + 0x198)
local out = {}
for slot = 0, 2 do
    local st = b + slot * 0x98
    local ip = rt.u64(st + 8)
    out[#out + 1] = string.format("slot %d stack %x item %x count %d aux %d", slot, st, ip, rt.u8(st + 0x22), rt.i32(st + 0x20) or 0)
    local cands = {ip, ip and rt.u64(ip)}
    for _, base in ipairs(cands) do
        if base and base > 0x10000 then
            for o = 0, 0x400, 8 do
                local q = rt.u64(base + o)
                local s
                if q and q > 0x10000 and q < 0x7fffffffffff then s = rt.cstr(q, 40) end
                local inl = rt.cstr(base + o, 40)
                if s and s:find("minecraft:") then out[#out + 1] = string.format("   [%x]+0x%x ptr-> %s", base, o, s) end
                if inl and inl:find("^minecraft:") then out[#out + 1] = string.format("   [%x]+0x%x inline %s", base, o, inl) end
                if inl and inl:find("^[a-z_]+$") and #inl > 3 then out[#out + 1] = string.format("   [%x]+0x%x word %s", base, o, inl) end
            end
        end
    end
end
rt.out("itemobj.txt", table.concat(out, "\n"))
rt.log("itemobj fin")
