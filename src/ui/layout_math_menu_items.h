#pragma once


#include <functional>
#include <string_view>
#include <vector>

#include "layout_common.h"

namespace layout_internal {
    struct MathMenuItem {
        std::string_view label;
        std::function<void(Editor&)> onSelect;
    };

    inline void OnMathFrac(Editor& /*editor*/) {
        // frac behavior
    }

    inline void OnMathSqrt(Editor& /*editor*/) {
        // sqrt behavior
    }

    inline void OnMathMat(Editor& /*editor*/) {
        // mat behavior
    }

    inline const std::vector<MathMenuItem>& MathMenuItems() {
        static const std::vector<MathMenuItem> items = {
            {"\\frac", OnMathFrac},
            {"\\sqrt", OnMathSqrt},
            {"\\mat",  OnMathMat},
        };
        return items;
    }
}
