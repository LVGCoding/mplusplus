#pragma once

// Text that mixes ordinary characters with math symbols, such as the
// message "x is in ℤ, but this is in ℝ". The body font has no ℤ or ℝ, so
// the text is split into runs and each run is drawn with a font that has it.

#include "clay.h"
#include "font.h"
#include "raylib.h"

#include <cstdint>
#include <string_view>

namespace rich_text {

// True for characters the body font is loaded with (see LoadFontTable):
// printable ASCII, Latin-1, lowercase Greek and a little punctuation.
inline bool InBodyFont(int codepoint) {
    return (codepoint >= 32 && codepoint < 127) || (codepoint >= 0xA0 && codepoint <= 0xFF) ||
           (codepoint >= 0x03B1 && codepoint <= 0x03C9) || codepoint == 0x2026 ||
           codepoint == 0x2212 || (codepoint >= 0x2018 && codepoint <= 0x201D);
}

// Calls emit(run, fontId) for each run of `text` that one font draws.
template <class Emit>
void ForEachRun(std::string_view text, std::uint16_t bodyFont, Emit emit) {
    std::size_t start = 0;
    std::uint16_t runFont = bodyFont;
    std::size_t at = 0;
    while (at < text.size()) {
        int length = 1;
        // GetCodepointNext reads at most 4 bytes; the text isn't NUL-terminated,
        // so only call it when a whole sequence fits.
        int codepoint = static_cast<unsigned char>(text[at]);
        if (codepoint >= 0x80 && at + 4 <= text.size()) {
            codepoint = GetCodepointNext(text.data() + at, &length);
        } else if (codepoint >= 0x80) {
            char buffer[5] = {};
            text.copy(buffer, text.size() - at, at);
            codepoint = GetCodepointNext(buffer, &length);
        }
        const std::uint16_t font = InBodyFont(codepoint) ? bodyFont : std::uint16_t{FONT_MATH};
        if (font != runFont && at > start) {
            emit(text.substr(start, at - start), runFont);
            start = at;
        }
        runFont = font;
        at += static_cast<std::size_t>(length > 0 ? length : 1);
    }
    if (at > start) {
        emit(text.substr(start, at - start), runFont);
    }
}

// Clay text elements for `text`, one per run, in the current element. Like
// CLAY_TEXT, `text` must stay alive until the frame is drawn. Put this in an
// element laid out left to right.
inline void Layout(std::string_view text, Clay_TextElementConfig config) {
    const std::uint16_t body = config.fontId;
    ForEachRun(text, body, [&](std::string_view run, std::uint16_t font) {
        Clay_TextElementConfig runConfig = config;
        runConfig.fontId = font;
        const Clay_String string = {.isStaticallyAllocated = false,
                                    .length = static_cast<int32_t>(run.size()),
                                    .chars = run.data()};
        Clay__OpenTextElement(string, runConfig);
    });
}

}  // namespace rich_text
