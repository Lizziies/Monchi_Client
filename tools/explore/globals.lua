rt.run("lib")

local ciVtable = BASE + 0x0e9731b0
local getLocalPlayer = rt.vtable(ciVtable, 600)[0x540 // 8 + 1]
local player, ci
for _, inst in ipairs(rt.heap(ciVtable, 8)) do
    local ok, lp = pcall(rt.call, getLocalPlayer, inst)
    if ok and lp ~= 0 and lp > 0x10000 then player, ci = lp, inst break end
end
if not player then rt.log("globals: no player") return end

local lines = {string.format("ci %x player %x", ci, player)}
local function refs(label, value)
    for _, at in ipairs(rt.findd(q2hex(value), 16)) do
        local users = rt.xrefs(at, 6)
        local fn = users[1] and select(1, rt.func(users[1])) or nil
        lines[#lines + 1] = string.format("%s in image data at %s, code refs %d%s", label, hexa(at), #users, fn and (" first fn " .. hexa(fn)) or "")
        for i = 1, math.min(#users, 3) do lines[#lines + 1] = "   " .. rt.disasm(users[i] - 3, 1):gsub("\n$", "") end
    end
end
refs("ClientInstance", ci)
refs("LocalPlayer", player)
-- objects that hold the ClientInstance and are themselves held by a global
for _, holder in ipairs(rt.heap(ci, 24)) do
    for _, at in ipairs(rt.findd(q2hex(holder & ~0xF), 2)) do
        lines[#lines + 1] = string.format("holder %x (ci at +%x of %x) referenced from image data %s", holder, holder & 0xF, holder & ~0xF, hexa(at))
    end
end
rt.out("globals.txt", table.concat(lines, "\n"))
rt.log("globals done")
