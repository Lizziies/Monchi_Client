#pragma once

#include <cmath>
#include <optional>

inline std::optional<int> hotbarSlot(float screenWidth, float left, float frameWidth) {
    if (!std::isfinite(screenWidth) || !std::isfinite(left) || !std::isfinite(frameWidth) ||
        screenWidth <= 0.f || frameWidth < 8.f || frameWidth > 256.f) return {};
    float scale = frameWidth / 24.f;
    // Bedrock's 24-pixel selection frame surrounds 20-pixel slots in a centered 182-pixel bar.
    float index = (left - (screenWidth * 0.5f - 92.f * scale)) / (20.f * scale);
    float rounded = std::round(index);
    if (rounded < 0.f || rounded > 8.f || std::abs(index - rounded) > 0.15f) return {};
    return int(rounded);
}
