rt.run("lib")
local pool = 0x208cf606010
local out = {}
for k = 0, 0x98, 8 do out[#out + 1] = string.format("  +%02x %x", k, rt.u64(pool + k) or 0) end
local pb, pe = rt.u64(pool + 0x20), rt.u64(pool + 0x28)
local ents = {}
for a = pb, pe - 4, 4 do ents[#ents + 1] = string.format("%x", rt.u32(a)) end
out[#out + 1] = "packed (" .. #ents .. "): " .. table.concat(ents, " ")
local sb, se = rt.u64(pool + 8), rt.u64(pool + 0x10)
out[#out + 1] = string.format("sparse pages %d first %x", (se - sb) // 8, rt.u64(sb) or 0)
local sp = rt.u64(sb)
local s = {}
for i = 0, 20 do s[#s + 1] = string.format("%x", rt.u32(sp + i * 4) or 0) end
out[#out + 1] = "sparse[0..20]: " .. table.concat(s, " ")
local page = rt.u64(rt.u64(pool + 0x50))
local q = {}
for i = 0, 0x100, 8 do q[#q + 1] = string.format("%x", rt.u64(page + i) or 0) end
out[#out + 1] = "payload: " .. table.concat(q, " ")
rt.out("pool7.txt", table.concat(out, "\n"))
rt.log("pool7 fin")
