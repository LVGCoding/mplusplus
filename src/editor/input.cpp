#include "input.h"

#include "raylib.h"

bool HandleInput(std::string& text){
    bool changed = false;
    int key = GetCharPressed();

    while (key > 0) {
        if (key >= 32 && key <= 126) {
            text += static_cast<char>(key);
            changed = true;
        }
        key = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE) && !text.empty()) {
        text.pop_back();
        changed = true;
    }
    return changed;
}
