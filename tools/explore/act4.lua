rt.run("lib")
local a = 0x207bee7c480
local player = rt.u64(BASE + 0x11d61a70)
local attrVt, healthDef = BASE + 0xe778140, BASE + 0x11dc54f0
local out = {}
local function str(at)
    local len, cap = rt.u64(at + 16) or 0, rt.u64(at + 24) or 0
    if len < 3 or len > 80 or cap < len or cap > 0x1000 then return nil end
    local s = rt.cstr(cap > 15 and rt.u64(at) or at, len)
    if s and s:match("^[%w_:%.%- ]+$") then return s end
end
for _, base in ipairs({{"target", a}, {"player", player}}) do
    for k = 0, 0x1800, 8 do
        local s = str(base[2] + k)
        if s then out[#out + 1] = string.format("%s +%x str '%s'", base[1], k, s) end
        local q = rt.u64(base[2] + k)
        if q and q > 0x10000 and q < 0x7fffffffffff then
            for j = 0, 0x40, 8 do
                if rt.u64(q + j) == attrVt then
                    for i = 0, 20 do if rt.u64(q + j + i * 0x88 + 8) == healthDef then out[#out + 1] = string.format("%s +%x -> attrs %x health %.1f", base[1], k, q + j, rt.f32(q + j + i * 0x88 + 0x7c)) end end
                    break
                end
            end
        end
    end
end
rt.out("act4.txt", table.concat(out, "\n"))
rt.log("act4 fin")
