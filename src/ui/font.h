#pragma once

#include "raylib.h"

#include <cstdint>

enum FontId : uint16_t { FONT_BODY = 0, FONT_BOLD = 1, FONT_COUNT };

// Load the fonts indexed by FontId; reload after a display DPI change.
void LoadFontTable(Font fonts[]);
// Call while the graphics context is still alive.
void UnloadFontTable(Font fonts[]);
