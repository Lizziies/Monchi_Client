rt.run("lib")
local er = rt.u64(rt.u64(BASE + 0x11d61a70) + 0x320)
local want = {
  [0x0e49cf3b]="UpdatePlayerFromCamera", [0xc7b5d956]="CameraBob", [0x113040a1]="ActiveCamera", [0xf6c60b46]="CurrentInputCamera",
  [0xceb578f1]="DirectLook", [0xf5b4eb5c]="LocalSpaceRotation", [0x2f0fc33f]="Orbit", [0x21ab4526]="PlayerBob", [0x4f6047c7]="Camera",
  [0xd5b28bf2]="PerspectiveOption", [0xed52b1c6]="FirstPerson", [0xab29c993]="ThirdPerson", [0xa8f0a7eb]="RedirectInput",
  [0x8bce84be]="DefaultInputCamera", [0x9ec7d9e5]="GameCamera", [0x119e772b]="RenderCamera", [0xce013bb9]="GameplayAffectsFov"}
local out = {}
local b, e = rt.u64(er + 0x98), rt.u64(er + 0xa0)
for n = b, e - 32, 32 do
    local key, pool = rt.u32(n + 8), rt.u64(n + 0x10)
    if want[key] then
        local pb, pe = rt.u64(pool + 0x20), rt.u64(pool + 0x28)
        local ents = {}
        for p = pb, pe - 4, 4 do ents[#ents + 1] = string.format("%x", rt.u32(p)) end
        local pages = rt.u64(pool + 0x50)
        local page = pages and rt.u64(pages) or 0
        out[#out + 1] = string.format("== %s ents %s page %x", want[key], table.concat(ents, ","), page)
        if page > 0x10000 then
            local h = rt.hex(page, 0x100) or ""
            for i = 1, #h, 96 do out[#out + 1] = "   " .. h:sub(i, i + 95) end
        end
    end
end
rt.out("cam.txt", table.concat(out, "\n"))
rt.log("cam fin")
