#pragma once

#include <cstdint>

#include "raylib.h"

enum FontId : uint16_t { 
  FONT_BODY = 0,
  FONT_BOLD = 1, 
  FONT_COUNT 
};

void LoadFontTable(Font fonts[]);
void UnloadFontTable(Font fonts[]);