#pragma once

#include "clay.h"
#include "editor/TextBuffer.h"
#include "raylib.h"
#include "ui/font.h"

#include <cstdint>
#include <string>
#include <string_view>

// A one-line text box for the UI, such as the math menu's search bar.
//
// The editing (cursor, selection, undo, clipboard, shortcuts) is a single-line
// TextBuffer, the same code the plain-text editor uses, so new editing
// features reach both. This class adds what a box needs: drawing in Clay with
// a placeholder and a blinking cursor, click-and-drag selection, and scrolling
// sideways when the text is wider than the box.
//
// Each frame: HandleKeyboard() (and HandleMouse()) before the layout, then
// Layout() inside the element that should hold the box.
class TextInput {
public:
    struct Style {
        uint16_t fontId = FONT_BODY;
        uint16_t fontSize = 18;
        Clay_Color textColor = {20, 20, 20, 255};
        Clay_Color placeholderColor = {150, 150, 150, 255};
        Clay_Color selectionColor = {65, 105, 168, 80};
        Clay_Color cursorColor = {20, 20, 20, 255};
        // Shown while the box is empty. Must stay alive until the frame renders.
        std::string_view placeholder;
        // The cursor floats over the text; give it the z index of the popup
        // or panel the box is in, plus one, so it draws on top.
        int16_t zIndex = 0;
    };

    // `id` names the box's Clay elements; it must be unique among text inputs.
    explicit TextInput(std::string id);

    TextBuffer buffer{/*singleLine=*/true};

    const std::string& Text() const { return buffer.text; }

    // Empty the box and forget its undo history.
    void Clear();

    // This frame's keyboard: typing (filtered by `accept`), Left/Right,
    // Backspace/Delete and the shortcuts. Up, Down, Enter, Tab and Escape are
    // left for the caller. Returns true if the text changed.
    bool HandleKeyboard(const TextBuffer::CharacterFilter& accept = {});

    // Click in the box to place the cursor, drag or Shift+click to select.
    // Uses last frame's layout. Returns true if the cursor or selection moved.
    bool HandleMouse(Font* fonts);

    // Emit the box: grows to fill its parent's width, one line tall.
    void Layout(const Style& style);

private:
    std::string id_;
    // Sideways scroll of the text (0 or negative), so the cursor stays in view.
    float scroll_ = 0.0f;
    bool dragging_ = false;
    // When the text or cursor last changed; the cursor stays solid briefly after.
    double lastActivity_ = 0.0;
    // Font settings from the last Layout(), for measuring mouse clicks.
    uint16_t fontId_ = FONT_BODY;
    uint16_t fontSize_ = 18;

    enum Part : uint32_t { Box, Row, CursorMarker, CursorBar, Selected };
    Clay_ElementId PartId(Part part) const;
    void ScrollCursorIntoView();
    void Touch() { lastActivity_ = GetTime(); }
};
