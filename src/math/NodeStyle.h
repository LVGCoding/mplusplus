#pragma once

// Shared look and Clay helpers for rendering EditorNodes.

#include "clay.h"

#include <algorithm>
#include <cmath>
#include <string_view>

namespace node_style {

inline constexpr Clay_Color textColor = {20, 20, 20, 255};
inline constexpr Clay_Color placeholderColor = {170, 170, 170, 255};

// Font size multipliers for nested content.
inline constexpr float fractionScale = 0.85f; // numerator and denominator
inline constexpr float indexScale = 0.6f;     // the n in \sqrt[n]{x}
inline constexpr float minFontSize = 8.0f;

// Widths of the drawn symbols, as a multiple of the font size.
inline constexpr float radicalWidthScale = 0.6f;
inline constexpr float delimiterWidthScale = 0.4f;

inline float Scaled(float fontSize, float scale) {
    return std::max(minFontSize, fontSize * scale);
}

// Thickness of fraction bars, radicals, and delimiters.
inline float RuleThickness(float fontSize) {
    return std::max(1.0f, std::round(fontSize / 18.0f));
}

// A run of text in the document font. `text` must stay alive until the frame
// has been rendered (node-owned strings or literals are fine).
void Text(std::string_view text, float fontSize);

// Grey box shown in place of an empty argument or cell, so it stays visible
// and clickable.
void Placeholder(float fontSize);

inline constexpr Clay_Color slotOutlineColor = {46, 160, 67, 255};

// Wraps one slot of a structure (a fraction's numerator, a matrix cell, ...).
// When `outlined`, it gets a green border; the padding is always there so the
// layout doesn't shift as the cursor moves in and out.
template <typename Body>
void Slot(bool outlined, Body&& body) {
    const Clay_BorderWidth width =
        outlined ? Clay_BorderWidth{1, 1, 1, 1, 0} : Clay_BorderWidth{0, 0, 0, 0, 0};
    CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                             .padding = CLAY_PADDING_ALL(1),
                             .childAlignment = {.x = CLAY_ALIGN_X_CENTER,
                                                .y = CLAY_ALIGN_Y_CENTER}},
                  .border = {.color = slotOutlineColor, .width = width}}) {
        body();
    }
}

// Custom elements that stretch to their laid-out height.
enum class Delimiter { Paren, Bracket, Brace, Bar, DoubleBar };
void Radical(float fontSize);
void DelimiterSymbol(Delimiter kind, bool right, float fontSize);

} // namespace node_style
