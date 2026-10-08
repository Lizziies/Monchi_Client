rt.run("lib")
rt.run("probe_skip")

-- local player via ClientInstance (vtable RVA and index from wave1..3, version 1.26.52.3)
local ciVtable = BASE + 0x0e9731b0
local getLocalPlayer = rt.vtable(ciVtable, 600)[0x540 // 8 + 1]
local player
for _, inst in ipairs(rt.heap(ciVtable, 8)) do
    local ok, lp = pcall(rt.call, getLocalPlayer, inst)
    if ok and lp ~= 0 and lp > 0x10000 then player = lp break end
end
if not player then rt.log("probe: no player") return end

local entries = rt.vtable(rt.u64(player), 800)
local report = {}
local function add(...) report[#report + 1] = string.format(...) end

-- only functions that are short and contain no store to memory are called
for i, fn in ipairs(entries) do
    local s, e = rt.func(fn)
    if s and e - s <= 120 then
        local text = rt.disfunc(fn)
        local stores = text:find("mov [a-z]* ?ptr %[[^\n]*%], ") or text:find("mov byte ptr %[") or text:find("lock ")
        local calls = select(2, text:gsub("call ", ""))
        if not stores and calls <= 2 and not SKIP[i - 1] then
            rt.log("try", i - 1)
            local okp, ptr = pcall(rt.call, fn, player)
            local okf, fl = pcall(rt.callf, fn, player)
            local line = string.format("%3d %s", i - 1, hexa(s))
            if okp and ptr and ptr > 0x10000 and ptr < 0x7fffffffffff then
                local a, b, c = rt.f32(ptr), rt.f32(ptr + 4), rt.f32(ptr + 8)
                line = line .. string.format("  ptr %x f32 %s %s %s", ptr, tostring(a), tostring(b), tostring(c))
            elseif okp then
                line = line .. string.format("  int %d", ptr)
            end
            if okf and fl and fl == fl and math.abs(fl) < 1e6 and fl ~= 0 then line = line .. string.format("  float %s", tostring(fl)) end
            add("%s", line)
        end
    end
end
rt.out("probe.txt", table.concat(report, "\n"))
rt.log("probe done", #report)
