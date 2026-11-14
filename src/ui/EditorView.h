#pragma once

#include "raylib.h"

#include <cmath>
#include <cstdint>

// Shared look of the document area. Used by the layout (gutter) and by each
// file type's rendering, so line numbers stay aligned with the text.
namespace editor_style {
inline constexpr uint16_t fontSize = 18;
inline constexpr uint16_t lineHeight = 23;
inline constexpr float padding = 6;
}  // namespace editor_style

// What a file needs to know about the editor area to render and handle input.
// Built by the layout each frame and passed to the active file's virtual
// rendering/input methods (see FileBase).
struct EditorView {
    // Loaded fonts, indexed by FontId (FONT_BODY, FONT_BOLD).
    Font* fonts = nullptr;

    // Screen bounds of the "Editor" element, with the scroll offset applied.
    // Text drawn at content.x/y + editor_style::padding lines up with Clay's text.
    Rectangle content = {};

    // The visible part of the editor. Clip overlays to this.
    Rectangle viewport = {};

    // False while the File menu is open, so clicks go to the menu instead.
    bool acceptsMouse = true;

    // Current blink phase of the text cursor.
    bool cursorVisible = true;
};

// Restrict drawing to the visible editor area. Pair with EndScissorMode().
inline void BeginEditorClip(const EditorView& view) {
    BeginScissorMode(static_cast<int>(std::ceil(view.viewport.x)),
                     static_cast<int>(std::ceil(view.viewport.y)),
                     static_cast<int>(std::floor(view.viewport.width)),
                     static_cast<int>(std::floor(view.viewport.height)));
}
