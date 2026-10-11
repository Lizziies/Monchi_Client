rt.run("lib")
local h = rt.find("55 41 57 41 56 41 55 41 54 56 57 53 48 81 EC 18 05 00 00 48 8D AC 24 80 00 00 00 0F 29 BD 80 04 00 00 0F 29 B5 70 04 00 00 48 C7 85 68 04 00 00 FE FF FF FF 4D 89 CE 4C 89 C6 48 89 D3 48 89 CF 48 8B 41 10 48 85 C0 74 0B 48 83 78 40 00 0F 85 ? ? ? ? 48 C7 45 C0 00 00 00 00 48 C7 45 D0 0F 00 00 00 48 C7 45 D8 00 00 00 00 48 C7 45 C8 0C 00 00 00 48 B8 75 69 5F 63 72 6F 73 73", 8)
rt.out("sigcheck.txt", string.format("cross %d %s", #h, h[1] and string.format("%x", h[1] - BASE) or "-"))
rt.log("sigcheck fin")
