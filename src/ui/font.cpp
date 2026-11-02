#include "font.h"

// Load the font table with selected fonts
void LoadFontTable(Font fonts[]){
  fonts[FONT_BODY] = LoadFontEx("assets/font/LiberationSans-Regular.ttf", 32, nullptr, 0);
  fonts[FONT_BOLD] = LoadFontEx("assets/font/LiberationSans-Bold.ttf", 32, nullptr, 0);
}

// Unload the font table from memory using Raylib's given function
void UnloadFontTable(Font fonts[]){
    for(int i = 0; i < FONT_COUNT; i++){
        UnloadFont(fonts[i]);
    }
}