#pragma once

#include <imgui.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace script {

struct Hud {
    std::string id;
    bool bar = false;
    std::string text;
    float x = 0.f;
    float y = 0.f;
    float w = 100.f;
    float h = 8.f;
    float fraction = 1.f;
    float scale = 1.f;
    int align = 0;
    bool shadow = true;
    bool background = false;
    ImVec4 color{1.f, 1.f, 1.f, 1.f};
    ImVec4 fill{0.f, 0.f, 0.f, 0.45f};
};

struct Info {
    std::string name;
    std::string error;
    bool enabled = true;
    bool running = false;
    int hud = 0;
    int handlers = 0;
    size_t memory = 0;
};

class Engine {
public:
    Engine();
    ~Engine();

    void setFolder(std::filesystem::path folder);
    void setAllowChat(bool allow);
    void setDisabled(const std::vector<std::string>& names);

    void scan(bool force);
    void tick(float dt);
    void keys(int vk, bool down);
    void serverChanged(const std::string& name, const std::string& host, bool joined);
    void events();
    void draw(ImDrawList* dl);

    std::vector<Info> list() const;
    void enable(const std::string& name, bool on);
    void reload(const std::string& name);
    void shutdown();

    struct Impl;

private:
    std::unique_ptr<Impl> impl_;
};

}
