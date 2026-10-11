rt.run("lib")
local out = {}
local hits = rt.bytes("pretty_function() [Type = ", false, 20000) or {}
for _, a in ipairs(hits) do
    local s = rt.cstr(a + 26, 300)
    if s then out[#out + 1] = (s:gsub("%]$", "")) end
end
rt.out("types.txt", table.concat(out, "\n"))
rt.log("types fin", #hits)
