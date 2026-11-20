#include "font.h"

#include "math/SymbolNode.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

void LoadFontTable(Font fonts[]) {
    // Rasterize at the display's pixel density; layout sizes stay in logical pixels.
    const float dpi = std::max(1.0f, GetWindowScaleDPI().y);
    const int atlasSize = static_cast<int>(std::ceil(32 * dpi));
    // Printable ASCII plus lowercase Greek (U+03B1..U+03C9) for GreekNode.
    static int codepoints[95 + 25];
    int count = 0;
    for (int codepoint = 32; codepoint < 127; ++codepoint) {
        codepoints[count++] = codepoint;
    }
    for (int codepoint = 0x03B1; codepoint <= 0x03C9; ++codepoint) {
        codepoints[count++] = codepoint;
    }
    fonts[FONT_BODY] =
        LoadFontEx("assets/font/LiberationSans-Regular.ttf", atlasSize, codepoints, count);
    fonts[FONT_BOLD] =
        LoadFontEx("assets/font/LiberationSans-Bold.ttf", atlasSize, codepoints, count);

    // The math font only draws SymbolNodes; load just their glyphs.
    static std::vector<int> symbolCodepoints = [] {
        std::vector<int> list;
        for (const auto& symbol : SymbolNode::Symbols()) {
            const std::string glyph(symbol.glyph);
            int length = 0;
            for (const char* at = glyph.c_str(); *at; at += length) {
                list.push_back(GetCodepointNext(at, &length));
            }
        }
        return list;
    }();
    fonts[FONT_MATH] = LoadFontEx("assets/font/NotoSansMath-Regular.ttf", atlasSize,
                                  symbolCodepoints.data(), static_cast<int>(symbolCodepoints.size()));

    for (int i = 0; i < FONT_COUNT; ++i) {
        SetTextureFilter(fonts[i].texture, TEXTURE_FILTER_BILINEAR);
    }
}

void UnloadFontTable(Font fonts[]) {
    for (int i = 0; i < FONT_COUNT; i++) {
        UnloadFont(fonts[i]);
    }
}
