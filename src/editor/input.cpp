#include "input.h"

#include "raylib.h"

#include <algorithm>

namespace {
    std::size_t PreviousCharacter(const std::string& text, std::size_t position) {
        if (position == 0) return 0;
        --position;
        while (position > 0 && (static_cast<unsigned char>(text[position]) & 0xC0) == 0x80) {
            --position;
        }
        return position;
    }

    std::size_t NextCharacter(const std::string& text, std::size_t position) {
        if (position == text.size()) return position;
        ++position;
        while (position < text.size() &&
               (static_cast<unsigned char>(text[position]) & 0xC0) == 0x80) {
            ++position;
        }
        return position;
    }
}

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
        if (key >= 32) {
            if (deleteSelection()) {
                changed = true;
            }

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
    if (IsKeyPressed(KEY_BACKSPACE) ||
        IsKeyPressedRepeat(KEY_BACKSPACE)) {

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
    if (IsKeyPressed(KEY_DELETE) ||
        IsKeyPressedRepeat(KEY_DELETE)) {

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
