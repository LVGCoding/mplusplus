#include "font.h"

#include <algorithm>
#include <cmath>

// Load the font table with selected fonts
void LoadFontTable(Font fonts[]){
  // Rasterize at the display's pixel density; layout sizes stay in logical pixels.
  const float dpi = std::max(1.0f, GetWindowScaleDPI().y);
  const int atlasSize = static_cast<int>(std::ceil(32 * dpi));
  fonts[FONT_BODY] = LoadFontEx("assets/font/LiberationSans-Regular.ttf", atlasSize, nullptr, 0);
  fonts[FONT_BOLD] = LoadFontEx("assets/font/LiberationSans-Bold.ttf", atlasSize, nullptr, 0);

  for (int i = 0; i < FONT_COUNT; ++i) {
    SetTextureFilter(fonts[i].texture, TEXTURE_FILTER_BILINEAR);
  }
}

// Unload the font table from memory using Raylib's given function
void UnloadFontTable(Font fonts[]){
    for(int i = 0; i < FONT_COUNT; i++){
        UnloadFont(fonts[i]);
    }
}
