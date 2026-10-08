-- Counts your kills per server and remembers the best streak between sessions.
local best = monchi.load("best", 0)
local streak = 0

monchi.on("kill", function(e)
  streak = streak + 1
  if streak > best then
    best = streak
    monchi.save("best", best)
    monchi.notify("New record", "Kill streak " .. best, "ok")
  end
end)

monchi.on("death", function()
  streak = 0
end)

monchi.on("tick", function(dt)
  monchi.hud.text("kills", "Streak " .. streak .. "  ·  best " .. best, 0.01, 0.40, { background = true })
end)
