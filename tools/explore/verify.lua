rt.run("lib")
rt.run("verify_tag")

local ciVtable = BASE + 0x0e9731b0
local getLocalPlayer = rt.vtable(ciVtable, 600)[0x540 // 8 + 1]
local player
for _, inst in ipairs(rt.heap(ciVtable, 8)) do
    local ok, lp = pcall(rt.call, getLocalPlayer, inst)
    if ok and lp ~= 0 and lp > 0x10000 then player = lp break end
end
if not player then rt.log("verify: no player") return end

local paths = {
    {0x28, 0x258, 0x5e0, 0x600},
    {0x38, 0x4f0, 0x828, 0xa60},
    {0x38, 0x4f0, 0xa58, 0x8f0},
    {0x48, 0x18, 0xef0, 0x420},
    {0x48, 0xf50, 0x430, 0x5a0},
    {0x50, 0xb68, 0x158, 0xc90},
}
local lines = {}
for _, p in ipairs(paths) do
    local a = player
    for i = 1, #p - 1 do a = rt.u64(a + p[i]) or 0 end
    a = a + p[#p]
    local desc = string.format("player>0x%x>0x%x>0x%x +0x%x", p[1], p[2], p[3], p[4])
    lines[#lines + 1] = string.format("%s  %s %s %s", desc, tostring(rt.f32(a)), tostring(rt.f32(a + 4)), tostring(rt.f32(a + 8)))
end
rt.out("verify_" .. (TAG or "x") .. ".txt", table.concat(lines, "\n"))
rt.log("verify done", TAG or "x")
