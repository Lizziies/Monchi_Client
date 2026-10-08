rt.run("lib")
local sh = 0x290f1ec0200
local out = {string.format("slot? %d  held+0x98 count %d", rt.i32(sh + 0x130), rt.u8(sh + 0x98 + 0x22))}
for k = 0x0, 0x40, 8 do out[#out + 1] = string.format("sh-0x%x %x", 0x3d8 - k, rt.u64(sh - 0x3d8 + k) or 0) end
local base = sh - 0x3d8
for o = 0, 0x600, 8 do local v = rt.u64(base + o); if v and v >= BASE and v < BASE + 0x14000000 then out[#out + 1] = string.format("base+0x%x vt %x", o, v - BASE) end end
out[#out + 1] = string.format("holder %x -> %x", sh - 0xc0, rt.u64(sh - 0xc0) or 0)
rt.out("chk3.txt", table.concat(out, "\n"))
rt.log("chk3 fin")
