rt.run("lib")

local ciVtable = BASE + 0x0e9731b0
local getLocalPlayer = rt.vtable(ciVtable, 600)[0x540 // 8 + 1]
local player
for _, inst in ipairs(rt.heap(ciVtable, 8)) do
    local ok, lp = pcall(rt.call, getLocalPlayer, inst)
    if ok and lp ~= 0 and lp > 0x10000 then player = lp break end
end
if not player then rt.log("chain: no player") return end

local lines = {string.format("player %x", player)}
local function q(label, base, from, to)
    for off = from, to, 8 do
        local v = rt.u64(base + off) or 0
        lines[#lines + 1] = string.format("%s+0x%03x  %016x  lo32 %d", label, off, v, v & 0xffffffff)
    end
end
q("player", player, 0x00, 0x60)
local r = rt.u64(player + 0x28)
q("[p+28]", r, 0x240, 0x270)
local a = rt.u64(r + 0x258)
q("[[p+28]+258]", a, 0x5c0, 0x600)
local b = rt.u64(a + 0x5e0)
q("[..+5e0]", b, 0x5e0, 0x620)
rt.out("chain.txt", table.concat(lines, "\n"))
rt.log("chain done")
