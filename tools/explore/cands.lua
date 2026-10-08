rt.run("lib")
rt.run("cands_tag")

local player = rt.u64(BASE + 0x11d61a70)
local blocks = {
    {"p>6a0>4e8", {0x6a0, 0x4e8}, 0xe80, 0xec0},
    {"p>470>ed8", {0x470, 0xed8}, 0x500, 0x590},
    {"p>38>1a8>e68", {0x38, 0x1a8, 0xe68}, 0x440, 0x480},
}
local lines = {}
for _, b in ipairs(blocks) do
    local a = player
    for _, o in ipairs(b[2]) do a = rt.u64(a + o) or 0 end
    for off = b[3], b[4], 4 do
        lines[#lines + 1] = string.format("%s +%03x %s", b[1], off, tostring(rt.f32(a + off)))
    end
end
rt.out("cands_" .. TAG .. ".txt", table.concat(lines, "\n"))
rt.log("cands done", TAG)
