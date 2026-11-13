#pragma once

// =====================================================================
//  MATH MENU ITEMS
//  This is the only file you need to touch to add, remove, reorder or
//  change what the math popup's buttons do.
//
//  To add an item:
//    1. Write an action function in the ACTIONS section below.
//    2. Add a {"label", ActionFunction} row in the ITEM LIST section.
//  The popup grows (and scrolls once it's taller than mathMenuMaxHeight
//  in layout_style.h) automatically.
// =====================================================================

#include <functional>
#include <string_view>
#include <vector>

#include "layout_common.h"

namespace layout_internal {
    struct MathMenuItem {
        // Text shown on the button. Must stay alive for the whole program
        // (a string literal is fine).
        std::string_view label;
        // Runs after the frame is drawn, when the button is clicked.
        // The popup closes right after (see ProcessMathMenuSelection).
        std::function<void(Editor&)> onSelect;
    };

    // ===== ACTIONS: what each button does =====
    // Each receives the Editor; use editor.ActiveFile() to reach the open file.
    // Lambdas work too, e.g. {"sum", [](Editor& editor) { /* ... */ }}.

    inline void OnMathFrac(Editor& /*editor*/) {
        // TODO: frac behavior
    }

    inline void OnMathSqrt(Editor& /*editor*/) {
        // TODO: sqrt behavior
    }

    inline void OnMathMat(Editor& /*editor*/) {
        // TODO: mat behavior
    }
    // ===== END ACTIONS =====

    inline const std::vector<MathMenuItem>& MathMenuItems() {
        static const std::vector<MathMenuItem> items = {
            // ===== ITEM LIST: shown top to bottom, add new rows here =====
            {"\\frac", OnMathFrac},
            {"\\sqrt", OnMathSqrt},
            {"\\mat",  OnMathMat},
            // ===== END ITEM LIST =====
        };
        return items;
    }
}
