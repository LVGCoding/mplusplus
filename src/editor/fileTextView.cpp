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

// Byte offset of the UTF-8 character before `position`.
std::size_t PreviousCharacter(const std::string& text, std::size_t position) {
    if (position == 0)
        return 0;
    --position;
    while (position > 0 && (static_cast<unsigned char>(text[position]) & 0xC0) == 0x80) {
        --position;
    }
    return position;
}

// Byte offset of the UTF-8 character after `position`.
std::size_t NextCharacter(const std::string& text, std::size_t position) {
    if (position == text.size())
        return position;
    ++position;
    while (position < text.size() && (static_cast<unsigned char>(text[position]) & 0xC0) == 0x80) {
        ++position;
    }
    return position;
}

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
    const bool changed = HandleKeyboard();
    if (changed) {
        modified = true;
    }
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

bool FileText::HandleKeyboard() {
    bool changed = false;

    cursorPos = std::min(cursorPos, text.size());
    selectionStart = std::min(selectionStart, text.size());

    const bool shiftHeld = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    const bool leftPressed = IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT);
    const bool rightPressed = IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT);

    // Move cursor left
    if (leftPressed) {
        if (!shiftHeld && selectionStart != cursorPos) {
            cursorPos = std::min(selectionStart, cursorPos);
        } else if (cursorPos > 0) {
            cursorPos = PreviousCharacter(text, cursorPos);
        }
        if (!shiftHeld) {
            selectionStart = cursorPos;
        }
    }

    // Move cursor right
    if (rightPressed) {
        if (!shiftHeld && selectionStart != cursorPos) {
            cursorPos = std::max(selectionStart, cursorPos);
        } else if (cursorPos < text.size()) {
            cursorPos = NextCharacter(text, cursorPos);
        }
        if (!shiftHeld) {
            selectionStart = cursorPos;
        }
    }

    // Delete currently selected text
    auto deleteSelection = [&]() {
        if (selectionStart == cursorPos) {
            return false;
        }
        const std::size_t start = std::min(selectionStart, cursorPos);
        const std::size_t end = std::max(selectionStart, cursorPos);
        text.erase(start, end - start);
        cursorPos = start;
        selectionStart = start;
        return true;
    };

    // Handle typed characters
    int key = GetCharPressed();
    while (key > 0) {
        if (key >= 32) {
            deleteSelection();
            int bytes = 0;
            const char* character = CodepointToUTF8(key, &bytes);
            text.insert(cursorPos, character, bytes);
            cursorPos += bytes;
            selectionStart = cursorPos;
            changed = true;
        }
        key = GetCharPressed();
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressedRepeat(KEY_ENTER)) {
        deleteSelection();
        text.insert(cursorPos, 1, '\n');
        ++cursorPos;
        selectionStart = cursorPos;
        changed = true;
    }

    // Handle Backspace
    if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
        if (deleteSelection()) {
            changed = true;
        } else if (cursorPos > 0) {
            const auto previous = PreviousCharacter(text, cursorPos);
            text.erase(previous, cursorPos - previous);
            cursorPos = previous;
            selectionStart = cursorPos;
            changed = true;
        }
    }

    // Handle Delete
    if (IsKeyPressed(KEY_DELETE) || IsKeyPressedRepeat(KEY_DELETE)) {
        if (deleteSelection()) {
            changed = true;
        } else if (cursorPos < text.size()) {
            text.erase(cursorPos, NextCharacter(text, cursorPos) - cursorPos);
            selectionStart = cursorPos;
            changed = true;
        }
    }

    return changed;
}
