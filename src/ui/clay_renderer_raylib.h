#pragma once

#include "clay.h"
#include "raylib.h"

// Raylib backend for clay. Font IDs used in Clay_TextElementConfig index into
// the fonts array passed to these functions.

Clay_Dimensions ClayRaylib_MeasureText(Clay_StringSlice text,
                                       Clay_TextElementConfig *config,
                                       void *userData);

// Custom Clay elements provide a drawing callback for their layout bounds.
struct ClayRaylibCustomDraw {
  void (*draw)(Clay_BoundingBox bounds);
};

void ClayRaylib_Render(Clay_RenderCommandArray commands, Font *fonts);
