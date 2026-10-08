rt.run("lib")

-- anchors: strings that sit inside a function (or name it), found by their text
local anchors = {
    {"tickWorld", "Player::tickWorld(const Tick"},
    {"mcUpdate", "MinecraftGame::update(void)"},
    {"mcUpdateInner", "MinecraftGame::_update(void)"},
    {"mcOnTick", "MinecraftGame::onTick(void)"},
    {"mcStartFrame", "MinecraftGame::startFrame(void)"},
    {"mcEndFrame", "MinecraftGame::endFrame(void)"},
    {"mcUpdateGraphics", "MinecraftGame::updateGraphics("},
    {"renderFrame", "GameRenderer::renderCurrentFrame(float)"},
    {"prepareFrame", "GameRenderer::_prepareFrame(ScreenContext"},
    {"extractFrame", "GameRenderer::_extractFrame("},
    {"preRenderUpdate", "LevelRendererPlayer::preRenderUpdate("},
    {"tickClouds", "LevelRendererPlayer::tickClouds("},
    {"leaveGame", "ClientInstance::requestLeaveGame(bool"},
    {"complexInv", "LocalPlayer::sendComplexInventoryTransaction setting up"},
    {"setContainerMgr", "Player::setContainerManagerModel("},
}

local report = {}
local function add(...) report[#report + 1] = table.concat({...}, " ") end

for _, a in ipairs(anchors) do
    local users = stringUsers(a[2], 12)
    add(string.format("== %s  (%d users)", a[1], #users))
    for i, u in ipairs(users) do
        local fnEnd = select(2, rt.func(u.fn))
        add(string.format("  fn %s size %d  string: %s", hexa(u.fn), fnEnd - u.fn, u.text:sub(1, 90)))
        local callers = rt.callers(u.fn, 12)
        add(string.format("    callers: %d", #callers))
        for _, vt in ipairs(vtablesWith(u.fn)) do
            add(string.format("    in vtable %s at index %d (entries %d)", hexa(vt.base), vt.index, vt.count))
        end
        if i == 1 then rt.out("fn_" .. a[1] .. ".txt", rt.disfunc(u.fn)) end
    end
end

rt.out("wave1.txt", table.concat(report, "\n"))
rt.log("wave1 done", #report)
