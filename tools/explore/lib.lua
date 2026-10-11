BASE = rt.base()

local codeRanges = {}
for _, s in ipairs(rt.sections()) do
    if s.exec then codeRanges[#codeRanges + 1] = s end
end

function hexa(a) return string.format("%08x", a - BASE) end

function isCode(a)
    for _, s in ipairs(codeRanges) do
        if a >= s.start and a < s.start + s.size then return true end
    end
    return false
end

function q2hex(v)
    local t = {}
    for i = 0, 7 do t[#t + 1] = string.format("%02x", (v >> (i * 8)) & 0xFF) end
    return table.concat(t, " ")
end

-- the whole C string that contains addr
function strAt(addr)
    local back = rt.hex(addr - 200, 200)
    local start = addr
    if back then
        start = addr - 200
        for i = 200, 1, -1 do
            if back:sub(i * 2 - 1, i * 2) == "00" then
                start = addr - 200 + i
                break
            end
        end
    end
    return start, rt.cstr(start, 400)
end

-- functions that reference a string constant: list of {fn=, ref=, text=}
function stringUsers(text, limit)
    local result, seen = {}, {}
    for _, hit in ipairs(rt.bytes(text, false, limit or 20)) do
        local s, full = strAt(hit)
        for _, ref in ipairs(rt.xrefs(s, 16)) do
            local fn = rt.func(ref)
            if fn and not seen[fn] then
                seen[fn] = true
                result[#result + 1] = {fn = fn, ref = ref, text = full}
            end
        end
    end
    return result
end

-- vtables that hold a function pointer: list of {base=, slot=, index=, count=}
function vtablesWith(fn)
    local result = {}
    for _, slot in ipairs(rt.findd(q2hex(fn), 32)) do
        local first = slot
        while isCode(rt.u64(first - 8) or 0) do first = first - 8 end
        local count = 0
        while isCode(rt.u64(first + count * 8) or 0) do count = count + 1 end
        result[#result + 1] = {base = first, slot = slot, index = (slot - first) // 8, count = count}
    end
    return result
end
