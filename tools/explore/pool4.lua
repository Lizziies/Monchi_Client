rt.run("lib")
local pool = 0x208cf5f7510
local hr = rt.u64(rt.u64(BASE + 0x11d61a70) + 0x1e8)
local out = {string.format("target %x", rt.u32(hr + 0x48))}
for k = 0, 0x98, 8 do out[#out + 1] = string.format("  +%02x %x", k, rt.u64(pool + k) or 0) end
local pb, pe = rt.u64(pool + 0x20), rt.u64(pool + 0x28)
local ents = {}
for a = pb, math.min(pe, pb + 400) - 4, 4 do ents[#ents + 1] = string.format("%x", rt.u32(a)) end
out[#out + 1] = "packed: " .. table.concat(ents, " ")
local page = rt.u64(rt.u64(pool + 0x50))
local ptrs = {}
for k = 0, 0x1960, 8 do local q = rt.u64(page + k); if q and q > 0x10000 then ptrs[#ptrs + 1] = string.format("%x:%x", k, q) end end
out[#out + 1] = "page ptrs: " .. table.concat(ptrs, " ")
rt.out("pool4.txt", table.concat(out, "\n"))
rt.log("pool4 fin")
