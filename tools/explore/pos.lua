rt.run("lib")
local function chain(start, offs)
    local p, s = start, string.format("%x", start or 0)
    for _, o in ipairs(offs) do
        if not p or p == 0 then break end
        p = rt.u64(p + o)
        s = s .. string.format(" >%x=%x", o, p or 0)
    end
    return p, s
end
local player = rt.u64(BASE + 0x11d61a70)
local _, s1 = chain(player, {0x138, 0x990})
local _, s2 = chain(player, {0x28, 0x258, 0x1f0})
local _, s3 = chain(player, {0x778, 0xb8})
local cam = select(1, chain(player, {0x138, 0x990}))
rt.log("player", s1, "|", s2, "|", s3)
if cam and cam ~= 0 then rt.log("pos", rt.f32(cam), rt.f32(cam + 4), rt.f32(cam + 8), rt.f32(cam + 12), rt.f32(cam + 16)) end
local sv = rt.u64(player + 0x1e8)
if sv then rt.log("sv", rt.f32(sv), rt.f32(sv + 4), rt.f32(sv + 8)) end
