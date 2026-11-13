// Rendering and input for plain text files (FileText).
// Loading and saving live in fileText.cpp.

#include "clay.h"
#include "fileText.h"
#include "raylib.h"
#include "ui/clay_renderer_raylib.h"
#include "ui/font.h"

#include <algorithm>
#include <cmath>

using editor_style::fontSize;
using editor_style::lineHeight;
using editor_style::padding;

namespace {
constexpr Clay_Color textColor = {20, 20, 20, 255};
constexpr Color cursorColor = {20, 20, 20, 255};
constexpr Color selectionColor = {65, 105, 168, 80};

// On-screen width of text[start, end) in the editor font.
float TextWidth(const std::string& text, std::size_t start, std::size_t end, Font font) {
    Clay_StringSlice slice = {.length = static_cast<int32_t>(end - start),
                              .chars = text.c_str() + start,
                              .baseChars = text.c_str()};
    Clay_TextElementConfig config = {.fontId = FONT_BODY, .fontSize = fontSize};
    return ClayRaylib_MeasureText(slice, &config, &font).width;
}

// Top-left screen position of the character at byte offset `position`.
Vector2 PositionOf(const std::string& text, std::size_t position, const EditorView& view) {
    position = std::min(position, text.size());
    const auto newline = position == 0 ? std::string::npos : text.rfind('\n', position - 1);
    const auto lineStart = newline == std::string::npos ? 0 : newline + 1;
    const auto row = std::count(text.begin(), text.begin() + position, '\n');
    return {view.content.x + padding + TextWidth(text, lineStart, position, view.fonts[FONT_BODY]),
            view.content.y + padding + row * lineHeight};
}
}  // namespace

// --- Layout -----------------------------------------------------------------

void FileText::LayoutContent() const {
    Clay_String clayText = {.isStaticallyAllocated = false,
                            .length = static_cast<int32_t>(text.size()),
                            .chars = text.c_str()};
    CLAY_TEXT(clayText,
              {.textColor = textColor,
               .fontId = FONT_BODY,
               .fontSize = fontSize,
               .lineHeight = lineHeight,
               .wrapMode = CLAY_TEXT_WRAP_NEWLINES});
}

std::size_t FileText::LineCount() const {
    return static_cast<std::size_t>(std::count(text.begin(), text.end(), '\n')) + 1;
}

// --- Drawing ----------------------------------------------------------------

void FileText::DrawOverlay(const EditorView& view) const {
    if (view.viewport.width <= 0 || view.viewport.height <= 0) {
        return;
    }
    DrawSelection(view);
    DrawCursor(view);
}

void FileText::DrawSelection(const EditorView& view) const {
    const auto start = std::min({selectionStart, cursorPos, text.size()});
    const auto end = std::min(std::max(selectionStart, cursorPos), text.size());
    if (start == end) {
        return;
    }
    const Font font = view.fonts[FONT_BODY];
    BeginEditorClip(view);
    std::size_t lineStart = 0;
    std::size_t row = 0;
    while (lineStart < end) {
        const auto newline = text.find('\n', lineStart);
        const auto lineEnd = newline == std::string::npos ? text.size() : newline;
        const auto from = std::max(start, lineStart);
        const auto to = std::min(end, lineEnd);
        const float y = view.content.y + padding + row * lineHeight;
        if (from <= to && y + lineHeight > view.viewport.y &&
            y < view.viewport.y + view.viewport.height) {
            const float x = view.content.x + padding + TextWidth(text, lineStart, from, font);
            float width = TextWidth(text, from, to, font);
            // Give a selected newline (including blank lines) a visible highlight.
            if (newline != std::string::npos && end > newline)
                width += fontSize / 2.0f;
            if (width > 0)
                DrawRectangleRec({x, y, width, float(lineHeight)}, selectionColor);
        }
        if (newline == std::string::npos)
            break;
        lineStart = newline + 1;
        ++row;
    }
    EndScissorMode();
}

void FileText::DrawCursor(const EditorView& view) const {
    if (!view.cursorVisible) {
        return;
    }
    const Vector2 cursor = PositionOf(text, cursorPos, view);
    BeginEditorClip(view);
    DrawRectangleRec({cursor.x, cursor.y, 2, float(lineHeight)}, cursorColor);
    EndScissorMode();
}

std::optional<Rectangle> FileText::CursorBounds(const EditorView& view) const {
    const Vector2 cursor = PositionOf(text, cursorPos, view);
    return Rectangle{cursor.x, cursor.y, 2, float(lineHeight)};
}

// --- Input ------------------------------------------------------------------

bool FileText::HandleInput(const EditorView& view) {
    const auto previousCursor = cursorPos;
    const auto previousAnchor = selectionStart;

    const bool clicked = HandleMouse(view);
    const bool changed = HandleKeyboard(view);
    return clicked || changed || previousCursor != cursorPos || previousAnchor != selectionStart;
}

bool FileText::HandleMouse(const EditorView& view) {
    if (!view.acceptsMouse || !IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        dragging_ = false;
        return false;
    }
    const Vector2 mouse = GetMousePosition();
    const bool clicked =
        IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mouse, view.viewport);
    if (clicked) {
        dragging_ = true;
    }
    if (!dragging_) {
        return false;
    }

    // Find the line under the mouse, then the character boundary nearest to it.
    const int row = std::max(
        0, static_cast<int>(std::floor((mouse.y - view.content.y - padding) / lineHeight)));
    std::size_t start = 0;
    for (int i = 0; i < row && start < text.size(); ++i) {
        const auto newline = text.find('\n', start);
        start = newline == std::string::npos ? text.size() : newline + 1;
    }
    const auto newline = text.find('\n', start);
    const auto end = newline == std::string::npos ? text.size() : newline;
    const float targetX = mouse.x - view.content.x - padding;
    const Font font = view.fonts[FONT_BODY];
    float x = 0;
    std::size_t position = start;
    while (position < end) {
        int bytes = 0;
        GetCodepointNext(text.c_str() + position, &bytes);
        const auto next = std::min(position + std::max(bytes, 1), end);
        const float width = TextWidth(text, position, next, font);
        if (targetX < x + width / 2)
            break;
        x += width;
        position = next;
    }

    cursorPos = position;
    if (clicked && !IsKeyDown(KEY_LEFT_SHIFT) && !IsKeyDown(KEY_RIGHT_SHIFT)) {
        selectionStart = position;
    }
    return clicked;
}

// Reads this frame's keys and passes them to PressKey/TypeCharacter
// (fileTextEdit.cpp), which do the editing.
bool FileText::HandleKeyboard(const EditorView& view) {
    const Font font = view.fonts[FONT_BODY];
    const LineMeasure measure = [font](std::string_view piece) {
        Clay_StringSlice slice = {.length = static_cast<int32_t>(piece.size()),
                                  .chars = piece.data(),
                                  .baseChars = piece.data()};
        Clay_TextElementConfig config = {.fontId = FONT_BODY, .fontSize = fontSize};
        Font copy = font;
        return ClayRaylib_MeasureText(slice, &config, &copy).width;
    };
    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    const bool shortcut = ShortcutModifierDown();  // Command on macOS
    auto pressed = [](int key) { return IsKeyPressed(key) || IsKeyPressedRepeat(key); };
    bool changed = false;

    if (shortcut && IsKeyPressed(KEY_A)) {
        SelectAll();
        changed = true;
    }
    // Modifier+Z undoes; Modifier+Y or Modifier+Shift+Z redoes. Held keys repeat.
    if (shortcut && pressed(KEY_Z)) {
        changed |= shift ? Redo() : Undo();
    }
    if (shortcut && pressed(KEY_Y)) {
        changed |= Redo();
    }
    for (const int key :
         {KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN, KEY_BACKSPACE, KEY_DELETE, KEY_ENTER}) {
        if (pressed(key)) {
            changed |= PressKey(key, shift, &measure);
        }
    }
    if (pressed(KEY_KP_ENTER)) {
        changed |= PressKey(KEY_ENTER, shift, &measure);
    }
    while (const int codepoint = GetCharPressed()) {
        changed |= TypeCharacter(static_cast<char32_t>(codepoint));
    }
    return changed;
}
