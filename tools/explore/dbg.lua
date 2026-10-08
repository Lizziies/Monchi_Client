rt.run("lib") local v = rt.u64(0x1e4cae3b3c0) rt.log("dbg", string.format("%x %x %x", BASE, BASE + 0x21b1c0, v or 0), #rt.heap(v, 100000))
