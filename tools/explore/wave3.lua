rt.run("lib")

local ciVtable = BASE + 0x0e9731b0
local ciEntries = rt.vtable(ciVtable, 600)
local getLocalPlayer = ciEntries[0x540 // 8 + 1]
rt.log("getLocalPlayer fn", hexa(getLocalPlayer))

local report = {}
local function add(...) report[#report + 1] = string.format(...) end

local player
for _, inst in ipairs(rt.heap(ciVtable, 8)) do
    local ok, lp = pcall(rt.call, getLocalPlayer, inst)
    add("ClientInstance %x -> localPlayer %s", inst, ok and string.format("%x", lp) or ("error " .. tostring(lp)))
    if ok and lp ~= 0 and not player then player = lp end
end

if player then
    local vt = rt.u64(player)
    add("LocalPlayer %x vtable %s", player, hexa(vt))
    local entries = rt.vtable(vt, 800)
    add("vtable entries %d", #entries)

    local idx, small = {}, {}
    for i, fn in ipairs(entries) do
        local s, e = rt.func(fn)
        if s then
            idx[#idx + 1] = string.format("%3d  %s  size %d", i - 1, hexa(s), e - s)
            if e - s <= 110 then
                small[#small + 1] = string.format("---- index %d fn %s size %d\n%s", i - 1, hexa(s), e - s, rt.disfunc(fn))
            end
        end
    end
    rt.out("lp_vtable_index.txt", table.concat(idx, "\n"))
    rt.out("lp_vtable_small.txt", table.concat(small, "\n"))

    local dump = {}
    for q = 0, 191 do
        local v = rt.u64(player + q * 8)
        local f = rt.f32(player + q * 8)
        local f2 = rt.f32(player + q * 8 + 4)
        dump[#dump + 1] = string.format("+%03x  %016x  f32 %-14s %-14s%s", q * 8, v or 0, tostring(f), tostring(f2), (v and isCode(v)) and "  code" or "")
    end
    rt.out("lp_object.txt", table.concat(dump, "\n"))
end

rt.out("wave3.txt", table.concat(report, "\n"))
rt.log("wave3 done")
