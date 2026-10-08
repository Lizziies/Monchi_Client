#include "sig/Database.hpp"
#include <cassert>

int main() {
    using nlohmann::json;
    json bundled = {{"sigs", {{"old", "before"}, {"ItemIconRender", "new"}, {"blocked", "unsafe"}}},
                    {"offsets", {{"player.totemAnimation", 4288}, {"old", 1}}}};
    json remote = {{"sigs", {{"old", "fixed"}, {"blocked", nullptr}}}, {"offsets", {{"old", 2}}}};
    auto merged = sigs::supplement(remote, bundled);
    assert(merged["sigs"]["ItemIconRender"] == "new");
    assert(merged["sigs"]["old"] == "fixed");
    assert(merged["sigs"]["blocked"].is_null());
    assert(merged["offsets"]["old"] == 2);
    assert(merged["offsets"]["player.totemAnimation"] == 4288);
    assert(sigs::supplement(merged, bundled) == merged);
    assert(sigs::supplement(json{{"sigs", nullptr}}, bundled)["sigs"].is_null());
}
