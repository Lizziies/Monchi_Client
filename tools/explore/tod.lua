rt.run("lib")
local out = {}
local seen = {}
for _, c in ipairs(rt.findd("00 80 BB 46", 64)) do
    for _, x in ipairs(rt.xrefs(c, 64)) do
        local f = rt.func(x)
        if f and not seen[f] then
            seen[f] = true
            local ok, d = pcall(rt.disfunc, f, 40)
            if ok and d and #d < 4000 then
                local n = select(2, d:gsub("\n", ""))
                out[#out + 1] = string.format("### func rva %x (lines %d) const %x", f - BASE, n, c - BASE)
                out[#out + 1] = d
            else
                out[#out + 1] = string.format("### func rva %x (big)", f - BASE)
            end
        end
    end
end
rt.out("tod.txt", table.concat(out, "\n"))
rt.log("tod fin", #out)
