rt.run("lib")
local f = BASE + 0xfb7880
local lines = {rt.hex(f, 0x50), rt.disasm(f, 12)}
local pat = "41 B8 32 00 00 00 FF 15 ? ? ? ? 48 8B 4C 24 28 48 89 C8 48 8B 49 08 48 8B 89 ? ? ? ? 48 85 C9 75 ED F3 0F 10 40 18"
for _, h in ipairs(rt.find(pat, 16)) do lines[#lines + 1] = string.format("hit %x  func %x  delta %x", h, rt.func(h) or 0, h - (rt.func(h) or h)) end
rt.out("mkgamma.txt", table.concat(lines, "\n"))
rt.log("mkgamma done")
