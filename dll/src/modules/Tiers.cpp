#include "Tiers.hpp"

#include <string_view>
#include <unordered_map>

namespace modules {

namespace {

constexpr std::string_view core[] = {
    "CPS", "FPS", "Ping Counter", "Keystrokes", "Armor HUD", "Potion HUD", "Coordinates", "Toggle Sprint",
    "Toggle Sneak", "Reach Counter", "Combo Counter", "Zoom", "Fullbright", "Freelook", "FOV Changer",
    "No Hurt Cam", "No View Bobbing", "Custom Crosshair", "Block Outline", "Auto GG",
    "Crystal Optimizer", "Hitbox", "Target HUD", "Pot Counter", "Totem Counter", "Hit Ping", "Low Health Indicator",
    "Scoreboard", "Low Latency", "Latency Meter", "Network Monitor", "Frame Limiter", "Performance Lock", "Mouse Sync", "Hive Utils",
};

constexpr std::string_view expected[] = {
    "Clock", "Direction HUD", "Speed Display", "Server Display", "IP Display", "Paperdoll", "Tab List",
    "Motion Blur", "Blur", "Shader Packs", "Waypoints", "Debug Menu", "Mouse Strokes", "Arrow Counter",
    "Item Counter", "Item Tracker", "Opponent Reach", "Waila",
    "Command Hotkey", "Text Hotkey", "Disable Mouse Wheel", "Java Dynamic FOV",
    "Death Logger",
    "Player Notifier", "Chunk Border", "Break Progress", "Cinematic Camera", "Snap Look", "Auto Perspective",
    "Sens Multiplier", "Bow Sensitivity", "Stopwatch", "Memory", "Experience Info", "Durability Warning",
    "Streamer Mode", "Server Profiles", "Better Chat", "View Model", "Saturation / Hue", "Screenshot+",
    "Session Timer", "Day Counter", "Hide Hand", "Entity Counter", "Zeqa Utils", "Kill Cleanup", "Inventory Lock", "Modern Keybind Handling", "Disable Inventory Hotkeys", "Nick", "Hotbar Animation",
    "TNT Timer", "Lua Scripts", "Config Sharing", "Monchi Online", "Fall Predictor", "Inventory Viewer", "Arrow Trail", "Black Bars",
    "Gamemode Hotkeys", "Third Person Nametag", "Health Above Head", "Hive Stats", "Hive Leaderboard",
    "Music", "Pitch Display",
};

constexpr std::string_view extras[] = {
    "Pet", "Petals", "Pomodoro", "Block Game", "Snake", "Flappy Heart", "DVD Screen", "20-20-20", "Watermark",
    "Kill Effects", "Hit Effects", "Hit Sound", "Totem Pop", "Totem Helper", "Match Summary", "Session Stats", "Hit Info",
    "Stats HUD", "Night Shift", "Sharpen", "Color Filter", "Brightness / Contrast", "Screen Tint", "Deepfry",
    "Upside Down", "Depth of Field", "Background Load", "Auto Profile", "Damage Indicator", "Hit Marker",
    "Bow Charge", "Cooldown Indicator",
};

constexpr std::string_view pvp[] = {
    "Combo Counter", "Reach Counter", "Opponent Reach", "Hit Ping",
    "Pot Counter", "Arrow Counter", "Totem Counter", "Target HUD", "Hitbox",
    "Low Health Indicator", "Auto GG", "Toggle Sprint", "Toggle Sneak", "Snap Look", "Crystal Optimizer", "CPS Limiter",
    "Hit Counter", "Null Movement", "Kill Cleanup",
};

const std::unordered_map<std::string_view, int>& table() {
    static const auto map = [] {
        std::unordered_map<std::string_view, int> m;
        for (auto n : core) m[n] = 1;
        for (auto n : expected) m[n] = 2;
        for (auto n : extras) m[n] = 4;
        return m;
    }();
    return map;
}

}

int displayCategory(const std::string& name) {
    for (auto n : pvp)
        if (n == name) return 2;
    return -1;
}

int tierOf(const std::string& name) {
    auto it = table().find(name);
    return it == table().end() ? 3 : it->second;
}

}
