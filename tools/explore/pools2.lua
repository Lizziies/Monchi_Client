rt.run("lib")
local player = rt.u64(BASE + 0x11d61a70)
local hr = rt.u64(player + 0x1e8)
local er = rt.u64(hr + 0x38)
local tid = rt.u32(hr + 0x48)
local b, e = rt.u64(er + 0x98), rt.u64(er + 0xa0)
local out = {string.format("target %x", tid)}
for n = b, e - 32, 32 do
    local key, pool = rt.u32(n + 8), rt.u64(n + 0x10)
    local vecs = {}
    for k = 8, 0x90, 8 do
        local vb, ve, vc = rt.u64(pool + k), rt.u64(pool + k + 8), rt.u64(pool + k + 16)
        if vb and ve and vc and vb > 0x10000 and ve >= vb and vc >= ve and vc - vb < 0x1000000 and (ve - vb) % 4 == 0 then vecs[#vecs + 1] = {k, vb, ve} end
    end
    local desc = {}
    local hasP, hasT = false, false
    for _, v in ipairs(vecs) do
        desc[#desc + 1] = string.format("+%x:%d", v[1], (v[3] - v[2]))
        local raw = rt.raw(v[2], math.min(v[3] - v[2], 0x40000))
        if raw then
            if raw:find(string.pack("<I4", tid), 1, true) then hasT = true end
            for i = 1, #raw - 3, 4 do if string.unpack("<I4", raw, i) == 1 then hasP = true break end end
        end
    end
    out[#out + 1] = string.format("key %08x pool %x vt %x P%s T%s %s", key, pool, (rt.u64(pool) or 0) - BASE, hasP and 1 or 0, hasT and 1 or 0, table.concat(desc, " "))
end
rt.out("pools2.txt", table.concat(out, "\n"))
rt.log("pools2 fin")
