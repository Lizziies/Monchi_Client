rt.run("lib")
local er = rt.u64(rt.u64(BASE + 0x11d61a70) + 0x320)
local function poolOf(key)
    local b, e = rt.u64(er + 0x98), rt.u64(er + 0xa0)
    for n = b, e - 32, 32 do if rt.u32(n + 8) == key then return rt.u64(n + 0x10) end end
end
local function page(key) return rt.u64(rt.u64(poolOf(key) + 0x50)) end
local upd = poolOf(0x0e49cf3b)
local look, rot = page(0xceb578f1), page(0x75df36b7)
local sp = rt.u64(rt.u64(upd + 8))
local slot = sp + 2 * 4
local orig = rt.u32(slot)
local function state(tag) rt.log(string.format("fl2 %s entry %x cam %.3f %.3f player %.1f %.1f", tag, rt.u32(slot), rt.f32(look), rt.f32(look + 4), rt.f32(rot), rt.f32(rot + 4))) end
state("before")
local bad = orig ~ 0x00040000
rt.wf32(slot, string.unpack("<f", string.pack("<I4", bad)))
rt.sleep(2500)
state("during")
rt.wf32(slot, string.unpack("<f", string.pack("<I4", orig)))
rt.sleep(200)
state("after")
