#include "font.h"

#include "math/BigOpNode.h"
#include "math/SymbolNode.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

void LoadFontTable(Font fonts[]) {
    // Rasterize at the display's pixel density; layout sizes stay in logical pixels.
    const float dpi = std::max(1.0f, GetWindowScaleDPI().y);
    const int atlasSize = static_cast<int>(std::ceil(32 * dpi));
    // Printable ASCII, Latin-1, lowercase Greek (U+03B1..U+03C9) for
    // GreekNode, and some punctuation for messages. Keep in sync with
    // rich_text::InBodyFont, which sends other characters to FONT_MATH.
    static int codepoints[95 + 96 + 25 + 8];
    int count = 0;
    for (int codepoint = 32; codepoint < 127; ++codepoint) {
        codepoints[count++] = codepoint;
    }
    for (int codepoint = 0xA0; codepoint <= 0xFF; ++codepoint) {
        codepoints[count++] = codepoint;
    }
    for (int codepoint = 0x03B1; codepoint <= 0x03C9; ++codepoint) {
        codepoints[count++] = codepoint;
    }
    for (int codepoint : {0x2026, 0x2212, 0x2018, 0x2019, 0x201A, 0x201B, 0x201C, 0x201D}) {
        codepoints[count++] = codepoint;
    }
    fonts[FONT_BODY] =
        LoadFontEx("assets/font/LiberationSans-Regular.ttf", atlasSize, codepoints, count);
    fonts[FONT_BOLD] =
        LoadFontEx("assets/font/LiberationSans-Bold.ttf", atlasSize, codepoints, count);

    // The math font only draws SymbolNodes and big operators; load just their glyphs.
    static std::vector<int> symbolCodepoints = [] {
        std::vector<int> list;
        std::vector<std::string_view> glyphs;
        for (const auto& symbol : SymbolNode::Symbols()) {
            glyphs.push_back(symbol.glyph);
        }
        for (const auto kind : {BigOpNode::Kind::SUM, BigOpNode::Kind::PRODUCT,
                                BigOpNode::Kind::UNION, BigOpNode::Kind::INTERSECTION}) {
            glyphs.push_back(BigOpNode::Glyph(kind));
        }
        for (const std::string_view symbolGlyph : glyphs) {
            const std::string glyph(symbolGlyph);
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
