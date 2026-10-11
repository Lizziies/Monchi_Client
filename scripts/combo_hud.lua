-- Shows your combo as a number with a small bar that empties after 1.5 seconds.
local last = 0

monchi.on("hit", function(e)
  last = monchi.time()
end)

monchi.on("tick", function(dt)
  local c = monchi.combat()
  if c.combo > 0 then
    local left = math.max(0, 1 - (monchi.time() - last) / 1.5)
    monchi.hud.text("combo", "Combo " .. c.combo, 0.5, 0.60, { color = 0xFF7DB5, scale = 1.4, align = "center" })
    monchi.hud.bar("combo_bar", 0.47, 0.645, 90, 5, left, { color = 0xFF7DB5 })
  else
    monchi.hud.remove("combo")
    monchi.hud.remove("combo_bar")
  end
end)
