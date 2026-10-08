rt.run("lib")

-- client instance vtable found through ClientInstance::requestLeaveGame (wave1)
local ciVtable = BASE + 0x0e9731b0
local entries = rt.vtable(ciVtable, 600)
rt.log("ClientInstance vtable entries", #entries)

local list, small = {}, {}
for i, fn in ipairs(entries) do
    local s, e = rt.func(fn)
    if s then
        local size = e - s
        list[#list + 1] = string.format("%3d  %s  size %d", i - 1, hexa(s), size)
        if size <= 96 then
            small[#small + 1] = string.format("---- index %d fn %s size %d\n%s", i - 1, hexa(s), size, rt.disfunc(fn))
        end
    end
end
rt.out("ci_vtable_index.txt", table.concat(list, "\n"))
rt.out("ci_vtable_small.txt", table.concat(small, "\n"))

local instances = rt.heap(ciVtable, 8)
rt.log("ClientInstance instances in heap", #instances)
local dump = {}
for _, inst in ipairs(instances) do
    dump[#dump + 1] = string.format("instance %x", inst)
    for q = 0, 63 do
        local v = rt.u64(inst + q * 8)
        dump[#dump + 1] = string.format("  +%03x  %016x%s", q * 8, v or 0, (v and isCode(v)) and "  code" or "")
    end
end
rt.out("ci_instances.txt", table.concat(dump, "\n"))
rt.log("wave2 done")
