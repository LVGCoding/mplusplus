#include "input.h"

#include "raylib.h"

void HandleInput(std::string& text){
    int key = GetCharPressed();

    while (key > 0) {
        if (key >= 32 && key <= 126) {
            text += static_cast<char>(key);
        }
        key = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE) && !text.empty()) {
        text.pop_back();
    }
}