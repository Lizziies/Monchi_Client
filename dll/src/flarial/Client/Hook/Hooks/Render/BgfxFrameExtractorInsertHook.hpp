// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Hook/Hooks/Render/BgfxFrameExtractorInsertHook.hpp
#pragma once

#include <array>
#include "../Hook.hpp"

class BgfxFrameExtractorInsertHook : public Hook {
public:
    BgfxFrameExtractorInsertHook();
    void enableHook() override;
};
