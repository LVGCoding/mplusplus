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

    // Magnifying glass for search bars.
    inline void DrawSearchIcon(Clay_BoundingBox bounds) {
        const Color color = {115, 115, 115, 255};
        const Vector2 center = {bounds.x + bounds.width / 2 - 2, bounds.y + bounds.height / 2 - 2};
        DrawRing(center, 4.5f, 6.0f, 0, 360, 24, color);
        DrawLineEx({center.x + 4.5f, center.y + 4.5f}, {center.x + 9, center.y + 9}, 2.0f, color);
    }

    inline ClayRaylibCustomDraw searchIcon = {DrawSearchIcon};
}
