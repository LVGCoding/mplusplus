#include "layout.h"

#include "clay.h"
#include "font.h"

void Layout(const std::string& EditorText){
    const Clay_Color background = {30, 30, 36, 255};
    const Clay_Color panel = {45, 45, 54, 255};
    const Clay_Color text = {230, 230, 235, 255};

    CLAY(CLAY_ID("Root"),
        {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                    .padding = CLAY_PADDING_ALL(16),
                    .childGap = 16},
                    .backgroundColor = background}) {
        CLAY(CLAY_ID("Sidebar"),
            {.layout = {.sizing = {CLAY_SIZING_FIXED(240), CLAY_SIZING_GROW(0)},
                        .padding = CLAY_PADDING_ALL(16),
                        .childGap = 8,
                        .layoutDirection = CLAY_TOP_TO_BOTTOM},
            .backgroundColor = panel,
            .cornerRadius = CLAY_CORNER_RADIUS(8)}) {
        CLAY_TEXT(CLAY_STRING("M++"),
                    {.textColor = text, .fontId = FONT_BOLD, .fontSize = 32});
        }
        CLAY(CLAY_ID("Editor"),
            {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                        .padding = CLAY_PADDING_ALL(16)},
            .backgroundColor = panel,
            .cornerRadius = CLAY_CORNER_RADIUS(8)}) {
        Clay_String clayText = {
            .isStaticallyAllocated = false,
            .length = static_cast<int32_t>(EditorText.size()),
            .chars = EditorText.c_str()
        };
        CLAY_TEXT(clayText,
                    {.textColor = text,
                    .fontId = FONT_BODY,
                    .fontSize = 20});
                    }
                }
}