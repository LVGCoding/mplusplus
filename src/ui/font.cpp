#include "font.h"

#include <algorithm>
#include <cmath>

// Load the font table with selected fonts
void LoadFontTable(Font fonts[]){
  // Rasterize at the display's pixel density; layout sizes stay in logical pixels.
  const float dpi = std::max(1.0f, GetWindowScaleDPI().y);
  const int atlasSize = static_cast<int>(std::ceil(32 * dpi));
  // Printable ASCII plus lowercase Greek (U+03B1..U+03C9) for GreekNode.
  static int codepoints[95 + 25];
  int count = 0;
  for (int c = 32; c < 127; ++c) codepoints[count++] = c;
  for (int c = 0x03B1; c <= 0x03C9; ++c) codepoints[count++] = c;
  fonts[FONT_BODY] = LoadFontEx("assets/font/LiberationSans-Regular.ttf", atlasSize, codepoints, count);
  fonts[FONT_BOLD] = LoadFontEx("assets/font/LiberationSans-Bold.ttf", atlasSize, codepoints, count);

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
