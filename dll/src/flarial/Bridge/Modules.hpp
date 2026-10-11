#pragma once

bool coreNeedsDraw();

// Module side of the MonchiFlarial.dll interface: Monchi lists Flarial's modules in its own menu, switches them,
// shows and edits their settings and tells the core which ones a server blocks. Plain C, like Bridge.hpp.
// Everything is called on Monchi's render thread, the same thread the core draws on.

struct MonchiFlarialModule {
    const char* name;
    const char* description;
    bool enabled;
};

// mirrors settingsRecorder::Kind
enum MonchiFlarialKind { MfHeader, MfText, MfButton, MfToggle, MfSlider, MfSliderInt, MfRangeSlider, MfTextBox, MfDropdown, MfColor, MfKeybind };

struct MonchiFlarialSetting {
    int kind;
    const char* label;
    const char* subtext;
    float min;
    float max;
    bool visible;
    int optionCount;
    const char* const* options;
};

// a setting's value; colors have three parts (hex text, opacity, rainbow), range sliders two (low, high)
struct MonchiFlarialValue {
    float number;
    bool flag;
    const char* text;
};

extern "C" {
using MonchiFlarialAbi = unsigned (*)();
using MonchiFlarialConfig = const char* (*)(int index);
using MonchiFlarialLoadConfig = bool (*)(int index, const char* text);
using MonchiFlarialModuleCount = int (*)();
using MonchiFlarialModuleAt = bool (*)(int index, MonchiFlarialModule* out);
// empty when every binding the module needs exists on the running game, else the missing signature names
using MonchiFlarialMissing = const char* (*)(int index);
using MonchiFlarialSetEnabled = void (*)(int index, bool on);
// a server rule (or a module Monchi already has) keeps the module off whatever Flarial's own toggles do
using MonchiFlarialSetBlocked = void (*)(int index, bool blocked);
// asks the module to describe its settings during the core's next frame; -1 until that happened
using MonchiFlarialRecord = void (*)(int index);
using MonchiFlarialSettingCount = int (*)(int index);
using MonchiFlarialSettingAt = bool (*)(int index, int setting, MonchiFlarialSetting* out);
using MonchiFlarialGet = bool (*)(int index, int setting, int part, MonchiFlarialValue* out);
using MonchiFlarialSet = bool (*)(int index, int setting, int part, const MonchiFlarialValue* value);
using MonchiFlarialPress = void (*)(int index, int setting);
// drops the core's module list held for Monchi; called before the core stops
using MonchiFlarialReleaseModules = void (*)();
}
