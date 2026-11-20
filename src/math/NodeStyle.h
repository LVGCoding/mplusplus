#pragma once

// Shared look and Clay helpers for rendering EditorNodes.

#include "clay.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string_view>

namespace node_style {

inline constexpr Clay_Color TEXT_COLOR = {20, 20, 20, 255};
inline constexpr Clay_Color PLACEHOLDER_COLOR = {170, 170, 170, 255};

// Font size multipliers for nested content.
inline constexpr float FRACTION_SCALE = 0.85f;  // numerator and denominator
inline constexpr float INDEX_SCALE = 0.6f;      // the n in \sqrt[n]{x}
inline constexpr float MIN_FONT_SIZE = 8.0f;

// Widths of the drawn symbols, as a multiple of the font size.
inline constexpr float RADICAL_WIDTH_SCALE = 0.6f;
inline constexpr float DELIMITER_WIDTH_SCALE = 0.4f;

inline float Scaled(float fontSize,
                    float scale) {
    return std::max(MIN_FONT_SIZE, fontSize * scale);
}

// Thickness of fraction bars, radicals, and delimiters.
inline float RuleThickness(float fontSize) {
    return std::max(1.0f, std::round(fontSize / 18.0f));
}

// A run of text in the document font (or `fontId`, e.g. FONT_MATH for
// symbols). `text` must stay alive until the frame has been rendered
// (node-owned strings or literals are fine).
void Text(std::string_view text,
          float fontSize,
          std::uint16_t fontId = 0);

// Grey box shown in place of an empty argument or cell, so it stays visible
// and clickable.
void Placeholder(float fontSize);

inline constexpr Clay_Color SLOT_OUTLINE_COLOR = {46, 160, 67, 255};

// Something marked to be deleted by the next Backspace.
inline constexpr Clay_Color DELETE_COLOR = {214, 40, 40, 255};
inline constexpr Clay_Color DELETE_FILL_COLOR = {214, 40, 40, 40};

// Wraps one slot of a structure (a fraction's numerator, a matrix cell, ...).
// When `outlined`, it gets a green border; the padding is always there so the
// layout doesn't shift as the cursor moves in and out.
template <typename Body>
void Slot(bool outlined,
          Body&& body) {
    const Clay_BorderWidth width =
        outlined ? Clay_BorderWidth{1, 1, 1, 1, 0} : Clay_BorderWidth{0, 0, 0, 0, 0};
    CLAY_AUTO_ID(
        {.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                    .padding = CLAY_PADDING_ALL(1),
                    .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER}},
         .border = {.color = SLOT_OUTLINE_COLOR, .width = width}}) {
        body();
    }
}

// Custom elements that stretch to their laid-out height.
enum class Delimiter { PAREN, BRACKET, BRACE, BAR, DOUBLE_BAR, FLOOR, CEIL };
void Radical(float fontSize);
void DelimiterSymbol(Delimiter kind,
                     bool right,
                     float fontSize);

// --- Object cards (graphs, tables) --------------------------------------------------

inline constexpr Clay_Color CARD_COLOR = {252, 252, 252, 255};
inline constexpr Clay_Color CARD_BORDER_COLOR = {200, 200, 200, 255};
inline constexpr Clay_Color MUTED_TEXT_COLOR = {140, 140, 140, 255};

// A small "Settings" button. Clicking it records `nodeId` for
// TakeSettingsRequest(), so the UI can open that node's settings menu.
void SettingsButton(std::uint32_t nodeId,
                    float fontSize);

// The id of the node whose Settings button was clicked since the last call.
std::optional<std::uint32_t> TakeSettingsRequest();

// A subtle button inside a node, like a table's "+" for a new row. Clicking it
// records (nodeId, action) for TakeActionRequest(); the UI then calls the
// node's RunAction(action) as one undo step. `label` must stay alive until
// the frame has been rendered.
void ActionButton(std::uint32_t nodeId,
                  int action,
                  std::string_view label,
                  float fontSize,
                  Clay_SizingAxis width,
                  Clay_SizingAxis height);

struct ActionRequest {
    std::uint32_t nodeId = 0;
    int action = 0;
};
// The action button clicked since the last call, if any.
std::optional<ActionRequest> TakeActionRequest();

}  // namespace node_style
