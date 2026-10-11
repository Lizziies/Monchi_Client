rt.run("lib")
-- every option: index in the Options array, save tag, raw value bytes; then every getter that asks for an option id
local player = rt.u64(BASE + 0x11d61a70)
local opts = rt.u64(rt.u64(player + 0x778) + 0xb8)
local function str(at)
    local len, cap = rt.u64(at + 16) or 0, rt.u64(at + 24) or 0
    if len == 0 or len > 200 then return "" end
    return rt.cstr(cap > 15 and rt.u64(at) or at, len) or ""
end
local lines = {}
for i = 0, 0x320 do
    local p = rt.u64(opts + 0x10 + i * 8)
    if p and p > 0x10000 then
    local info = rt.u64(p + 8)
    local name = info and str(info + 0x188) or "?"
    lines[#lines + 1] = string.format("opt %3d (0x%x)  %-34s id %s  raw %s  f18=%.3f", i, i, name, info and tostring(rt.i32(info + 0x1cc)) or "?", rt.hex(p + 0x10, 16) or "", rt.f32(p + 0x18) or 0)
    end
end
local pat = "41 B8 ? ? 00 00 FF 15 ? ? ? ? 48 8B 4C 24 28 48 89 C8 48 8B 49 08 48 8B 89 ? ? ? ? 48 85 C9 75 ED"
for _, h in ipairs(GETTERS and rt.find(pat, 4000) or {}) do
    local f = rt.func(h) or 0
    lines[#lines + 1] = string.format("get id 0x%x  func rva %x  delta %x  tail %s", rt.u16(h + 2) or 0, f - BASE, h - f, rt.hex(h + 36, 10) or "")
end
rt.out("optmap.txt", table.concat(lines, "\n"))
rt.log("optmap done", #lines)

