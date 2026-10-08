-- watches the first bytes of dxgi Present to see whether another overlay rewrites our hook
local present = 0x7ffcc8159530
for i = 1, 8 do
    rt.log("present bytes", i, rt.hex(present, 16))
    rt.sleep(700)
end
