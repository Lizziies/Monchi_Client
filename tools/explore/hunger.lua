rt.run("lib")
local out = {}
for _, txt in ipairs({"minecraft:player.hunger", "minecraft:player.saturation", "minecraft:player.level", "minecraft:player.experience", "minecraft:player.exhaustion"}) do
    for _, s in ipairs(rt.bytes(txt, false, 4)) do
        out[#out + 1] = string.format("%s str rva %x", txt, s - BASE)
        for _, x in ipairs(rt.xrefs(s, 6)) do
            local f = rt.func(x)
            out[#out + 1] = string.format("   xref %x func %s  %s", x - BASE, f and string.format("%x", f - BASE) or "?", rt.disasm(x, 3):gsub("\n", " | "))
        end
    end
end
rt.out("hunger.txt", table.concat(out, "\n"))
rt.log("hunger fin")
