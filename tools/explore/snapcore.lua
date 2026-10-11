rt.run("lib")

local ciVtable = BASE + 0x0e9731b0
local getLocalPlayer = rt.vtable(ciVtable, 600)[0x540 // 8 + 1]

local player
for _, inst in ipairs(rt.heap(ciVtable, 8)) do
    local ok, lp = pcall(rt.call, getLocalPlayer, inst)
    if ok and lp ~= 0 then
        player = lp
        break
    end
end
if not player then
    rt.log("snap", TAG, "no local player")
    return
end

local lpVtable = rt.u64(player)
local state = rt.call(rt.vtable(lpVtable, 800)[0x560 // 8 + 1], player)
rt.log("snap", TAG, string.format("player %x state %x", player, state))

local lines = {string.format("player %x state %x", player, state)}
for off = 0, 0x5f8, 4 do
    local f = rt.f32(state + off)
    local i = rt.i32(state + off)
    lines[#lines + 1] = string.format("+%03x  %-16s %d", off, tostring(f), i or 0)
end
rt.out("snap_" .. TAG .. ".txt", table.concat(lines, "\n"))
