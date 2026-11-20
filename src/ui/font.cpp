#include "font.h"

#include <algorithm>
#include <cmath>

void LoadFontTable(Font fonts[]) {
    // Rasterize at the display's pixel density; layout sizes stay in logical pixels.
    const float dpi = std::max(1.0f, GetWindowScaleDPI().y);
    const int atlasSize = static_cast<int>(std::ceil(32 * dpi));
    // Printable ASCII, lowercase Greek, and uppercase sigma for SumNode.
    static int codepoints[95 + 25 + 1];
    int count = 0;
    for (int codepoint = 32; codepoint < 127; ++codepoint) {
        codepoints[count++] = codepoint;
    }
    for (int codepoint = 0x03B1; codepoint <= 0x03C9; ++codepoint) {
        codepoints[count++] = codepoint;
    }
    codepoints[count++] = 0x03A3;
    fonts[FONT_BODY] =
        LoadFontEx("assets/font/LiberationSans-Regular.ttf", atlasSize, codepoints, count);
    fonts[FONT_BOLD] =
        LoadFontEx("assets/font/LiberationSans-Bold.ttf", atlasSize, codepoints, count);

    for (int i = 0; i < FONT_COUNT; ++i) {
        SetTextureFilter(fonts[i].texture, TEXTURE_FILTER_BILINEAR);
    }
}

void UnloadFontTable(Font fonts[]) {
    for (int i = 0; i < FONT_COUNT; i++) {
        UnloadFont(fonts[i]);
    }
}
