#pragma once

// Custom-drawn icons rendered through the Clay raylib renderer.

#include "layout_common.h"

namespace layout_internal {
    inline void DrawChevron(Clay_BoundingBox bounds) {
        const float x = bounds.x;
        const float y = bounds.y + bounds.height / 2;
        const Color color = {115, 115, 115, 255};
        DrawLineEx({x + 1, y - 3}, {x + 6, y + 2}, 1.5f, color);
        DrawLineEx({x + 6, y + 2}, {x + 11, y - 3}, 1.5f, color);
    }

    inline ClayRaylibCustomDraw chevron = {DrawChevron};

    inline void DrawCloseIcon(Clay_BoundingBox bounds) {
        const float x = bounds.x + bounds.width / 2;
        const float y = bounds.y + bounds.height / 2;
        DrawLineEx({x - 5, y - 5}, {x + 5, y + 5}, 1.5f, BLACK);
        DrawLineEx({x - 5, y + 5}, {x + 5, y - 5}, 1.5f, BLACK);
    }

    inline ClayRaylibCustomDraw closeIcon = {DrawCloseIcon};
}
