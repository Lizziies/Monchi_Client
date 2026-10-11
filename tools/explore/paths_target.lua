local sv = rt.u64(rt.u64(BASE + 0x11d61a70) + 0x1e8)
TARGET = {rt.f32(sv), rt.f32(sv + 4), rt.f32(sv + 8)}
TOL = 0.001
TAG = 'look3'
MAXNODES = 30000
SPAN = 0x1000
ROOTSPAN = 0x2000
MAXDEPTH = 3
