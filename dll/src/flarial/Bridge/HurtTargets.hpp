#pragma once
namespace hurtTargets {
enum class Action { Keep, Hide, Color };
constexpr Action action(bool known, bool self, bool colorSelf, bool colorOthers) {
    if (!known) return Action::Keep;
    return (self ? colorSelf : colorOthers) ? Action::Color : Action::Hide;
}
constexpr const char* key(bool self) {
    return self ? "selfHurt" : "hurt";
}
}
