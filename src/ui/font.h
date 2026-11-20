#pragma once

#include "raylib.h"

#include <cstdint>

// FONT_MATH (Noto Sans Math) draws the symbols the text fonts lack (∈, ℝ, ←, ...).
enum FontId : uint16_t { FONT_BODY = 0, FONT_BOLD = 1, FONT_MATH = 2, FONT_COUNT };

// How to draw a font so its letters match the body font's size and baseline.
// raylib sizes a font by its whole line height (ascent + descent), and Noto
// Sans Math's is much taller than Liberation Sans's, to fit big operators, so
// at the same size its symbols come out a quarter smaller and sit too low.
// Scale it up to the same em size and move it up to the same baseline; the
// text box keeps the body font's height, so rows don't grow.
struct FontAdjustment {
    float scale = 1.0f;
    // Added to the drawing position, in units of the font size.
    float shiftY = 0.0f;
};

inline FontAdjustment AdjustmentFor(uint16_t fontId) {
    if (fontId != FONT_MATH) {
        return {};
    }
    // From the fonts' head/hhea tables: (ascent - descent) / unitsPerEm is
    // 1.492 for Noto Sans Math and 1.1172 for Liberation Sans; their baselines
    // sit at ascent / (ascent - descent) = 0.7165 and 0.8103 of the size.
    constexpr float scale = 1.492f / 1.1171875f;
    return {scale, 0.8103f - 0.7165f * scale};
}

// Load the fonts indexed by FontId; reload after a display DPI change.
void LoadFontTable(Font fonts[]);
// Call while the graphics context is still alive.
void UnloadFontTable(Font fonts[]);
