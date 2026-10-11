-- Turns a warning on screen when your ping goes over 120 ms.
local limit = 120

monchi.on("tick", function(dt)
  local w = monchi.world()
  if w.ping > limit then
    monchi.hud.text("ping", "High ping: " .. w.ping .. " ms", 0.5, 0.12, { color = 0xFF6B73, scale = 1.3, align = "center", background = true })
  else
    monchi.hud.remove("ping")
  end
end)
