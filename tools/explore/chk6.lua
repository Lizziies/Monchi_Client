rt.run("lib") local p = rt.u64(BASE + 0x11d61a70) local gd = rt.u64(rt.u64(rt.u64(p + 0x778) + 0xb0) + 0x650) local b, e = rt.u64(gd + 0x150), rt.u64(gd + 0x158) rt.log("chk6 n", (e - b) / 0x110)
