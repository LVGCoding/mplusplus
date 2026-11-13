#pragma once

// Colors and sizes used by the layout.

#include "layout_common.h"

namespace layout_internal {
    inline constexpr Clay_Color menuText = {20, 20, 20, 255};
    inline constexpr Clay_Color menuHover = {205, 205, 205, 255};

    // Look sampled from UIMockup1.pptx
    inline constexpr Clay_Color titleBarColor = {78, 167, 46, 255};
    inline constexpr Clay_Color menuBarColor = {232, 232, 232, 255};
    inline constexpr Clay_Color dropdownColor = {209, 209, 209, 255};
    inline constexpr Clay_Color gutterColor = {232, 232, 232, 255};
    inline constexpr Clay_Color dividerColor = {209, 209, 209, 255};
    inline constexpr Clay_Color editorColor = {255, 255, 255, 255};
    inline constexpr uint16_t editorFontSize = editor_style::fontSize;
    inline constexpr uint16_t editorLineHeight = editor_style::lineHeight;
    inline constexpr float gutterWidth = 46;
    inline constexpr Clay_Color terminalBorderColor = {174, 174, 174, 255};
    inline constexpr int gutterMinLines = 20;
    inline constexpr float editorPadding = editor_style::padding;

    // Math popup size. The item list scrolls (mouse wheel / trackpad) once it is
    // taller than mathMenuListMaxHeight; the search bar stays fixed above it.
    inline constexpr float mathMenuWidth = 320;
    inline constexpr float mathMenuListMaxHeight = 300;
    inline constexpr float mathMenuItemHeight = 34;
    inline constexpr float mathMenuCornerRadius = 12;
    inline constexpr float mathMenuSearchHeight = 34;
    inline constexpr uint16_t mathMenuFontSize = 20;
}
