rt.run("lib")
local str = 0x224848d63c0
local secs = rt.sections()
local lines = {}
-- candidate object starts: qwords before the string that point into the game image (vtables)
local starts = {}
for back = 8, 0x600, 8 do
    local q = rt.u64(str - back)
    if q and q >= BASE and q < BASE + 0x14000000 then starts[#starts + 1] = str - back; lines[#lines + 1] = string.format("image ptr at str-0x%x -> rva %x", back, q - BASE) end
end
for _, s in ipairs(starts) do
    local refs = rt.heap(s, 12)
    for _, r in ipairs(refs) do lines[#lines + 1] = string.format("  ref to %x (str-0x%x) at %x", s, str - s, r) end
end
rt.out("item2.txt", table.concat(lines, "\n"))
rt.log("item2 fin", #lines)
