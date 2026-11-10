// Rendering and input for M++ documents (FileMpp).
// Loading and saving live in fileMpp.cpp.
//
// TODO: these are placeholders that show the raw source read-only. Replace them
// with node rendering/editing:
//   - LayoutContent(): call Render() on each node in `content`
//   - HandleInput():   route keys/clicks to the node under the cursor (KeyPress/MouseClick)
//   - DrawOverlay():   draw the cursor/selection using cursorPos/selectionStart
//   - CursorBounds():  return the cursor rectangle so the editor can scroll to it

#include "FileMpp.h"
#include "clay.h"
#include "ui/font.h"

#include <algorithm>

bool FileMpp::HandleInput(const EditorView& /*view*/) {
    return false;
}

void FileMpp::LayoutContent() const {
    Clay_String clayText = {.isStaticallyAllocated = false,
                            .length = static_cast<int32_t>(source.size()),
                            .chars = source.c_str()};
    CLAY_TEXT(clayText,
              {.textColor = {20, 20, 20, 255},
               .fontId = FONT_BODY,
               .fontSize = editor_style::fontSize,
               .lineHeight = editor_style::lineHeight,
               .wrapMode = CLAY_TEXT_WRAP_NEWLINES});
}

std::size_t FileMpp::LineCount() const {
    return static_cast<std::size_t>(std::count(source.begin(), source.end(), '\n')) + 1;
}

void FileMpp::DrawOverlay(const EditorView& /*view*/) const {}

std::optional<Rectangle> FileMpp::CursorBounds(const EditorView& /*view*/) const {
    return std::nullopt;
}
