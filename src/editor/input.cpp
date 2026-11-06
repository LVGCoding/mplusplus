#include "input.h"

#include "raylib.h"

#include <algorithm>

bool HandleInput(std::string& text,
                 std::size_t& cursorPos,
                 std::size_t& selectionStart) {

    bool changed = false;

    cursorPos = std::min(cursorPos, text.size());
    selectionStart = std::min(selectionStart, text.size());

    bool shiftHeld = IsKeyDown(KEY_LEFT_SHIFT) ||
                     IsKeyDown(KEY_RIGHT_SHIFT);

    bool leftPressed = IsKeyPressed(KEY_LEFT) ||
                       IsKeyPressedRepeat(KEY_LEFT);

    bool rightPressed = IsKeyPressed(KEY_RIGHT) ||
                        IsKeyPressedRepeat(KEY_RIGHT);

    // Move cursor left
    if (leftPressed) {
        if (!shiftHeld && selectionStart != cursorPos) {
            cursorPos = std::min(selectionStart, cursorPos);
        } else if (cursorPos > 0) {
            cursorPos--;
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
            cursorPos++;
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

        std::size_t start = std::min(selectionStart, cursorPos);
        std::size_t end = std::max(selectionStart, cursorPos);

        text.erase(start, end - start);
        cursorPos = start;
        selectionStart = start;

        return true;
    };

    // Handle typed characters
    int key = GetCharPressed();

    while (key > 0) {
        if (key >= 32 && key <= 126) {
            if (deleteSelection()) {
                changed = true;
            }

            text.insert(cursorPos, 1, static_cast<char>(key));
            cursorPos++;
            selectionStart = cursorPos;
            changed = true;
        }

        key = GetCharPressed();
    }

    // Handle Backspace
    if (IsKeyPressed(KEY_BACKSPACE) ||
        IsKeyPressedRepeat(KEY_BACKSPACE)) {

        if (deleteSelection()) {
            changed = true;
        } else if (cursorPos > 0) {
            text.erase(cursorPos - 1, 1);
            cursorPos--;
            selectionStart = cursorPos;
            changed = true;
        }
    }

    // Handle Delete
    if (IsKeyPressed(KEY_DELETE) ||
        IsKeyPressedRepeat(KEY_DELETE)) {

        if (deleteSelection()) {
            changed = true;
        } else if (cursorPos < text.size()) {
            text.erase(cursorPos, 1);
            selectionStart = cursorPos;
            changed = true;
        }
    }

    return changed;
}