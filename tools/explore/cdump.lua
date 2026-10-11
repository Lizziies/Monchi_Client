rt.run("lib")
local er = rt.u64(rt.u64(BASE + 0x11d61a70) + 0x320)
local want = {
  [0x1b5d5238]="StateVector", [0xbac1b3cf]="AABBShape", [0xe6a1b550]="MobEffects", [0x97d981a9]="Nameable",
  [0x85b93800]="ActorOwner", [0xfc0dbbb5]="RuntimeID", [0xd7a3585c]="MobHurtTime", [0xf95d258f]="Player",
  [0xacb47b38]="LocalPlayer", [0xc67426f3]="ActorDataFlag", [0xe642016b]="SynchedActorData", [0x4f6ba419]="ActorType",
  [0x18f957af]="UniqueID", [0xe53c7221]="RenderPosition", [0xd15944e2]="RenderRotation", [0x75df36b7]="ActorRotation",
  [0xdeb6534f]="Identifier", [0xbabe7211]="HeadRotation", [0xb06141a9]="Equipment"}
local out = {}
local b, e = rt.u64(er + 0x98), rt.u64(er + 0xa0)
for n = b, e - 32, 32 do
    local key, pool = rt.u32(n + 8), rt.u64(n + 0x10)
    if want[key] then
        local pb, pe = rt.u64(pool + 0x20), rt.u64(pool + 0x28)
        local ents = {}
        for p = pb, pe - 4, 4 do ents[#ents + 1] = string.format("%x", rt.u32(p)) end
        local pages = rt.u64(pool + 0x50)
        local page = pages and rt.u64(pages)
        out[#out + 1] = string.format("== %s pool %x ents %s page %x", want[key], pool, table.concat(ents, ","), page or 0)
        if page and page > 0x10000 then
            local h = rt.hex(page, 0x180) or ""
            for i = 1, #h, 96 do out[#out + 1] = "   " .. h:sub(i, i + 95) end
        end
    end
end
rt.out("cdump.txt", table.concat(out, "\n"))
rt.log("cdump fin")

