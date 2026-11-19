#pragma once

// Colors and sizes used by the layout.

#include "layout_common.h"

namespace layout_internal {
inline constexpr Clay_Color MENU_TEXT = {20, 20, 20, 255};
inline constexpr Clay_Color MENU_HOVER = {205, 205, 205, 255};

// Look sampled from UIMockup1.pptx
inline constexpr Clay_Color TITLE_BAR_COLOR = {78, 167, 46, 255};
inline constexpr Clay_Color MENU_BAR_COLOR = {232, 232, 232, 255};
inline constexpr Clay_Color DROPDOWN_COLOR = {209, 209, 209, 255};
inline constexpr Clay_Color GUTTER_COLOR = {232, 232, 232, 255};
inline constexpr Clay_Color DIVIDER_COLOR = {209, 209, 209, 255};
inline constexpr Clay_Color EDITOR_COLOR = {255, 255, 255, 255};
inline constexpr uint16_t EDITOR_FONT_SIZE = editor_style::FONT_SIZE;
inline constexpr uint16_t EDITOR_LINE_HEIGHT = editor_style::LINE_HEIGHT;
inline constexpr float GUTTER_WIDTH = 46;
inline constexpr Clay_Color TERMINAL_BORDER_COLOR = {174, 174, 174, 255};
inline constexpr Clay_Color TERMINAL_HANDLE_HOVER_COLOR = {120, 120, 120, 255};

// Settings popups and their forms (layout_form.h).
inline constexpr float FORM_LABEL_WIDTH = 52;
inline constexpr float FORM_INPUT_HEIGHT = 24;
inline constexpr uint16_t FORM_FONT_SIZE = 14;
inline constexpr uint16_t FORM_HINT_FONT_SIZE = 13;
inline constexpr uint16_t FORM_TITLE_FONT_SIZE = 16;
inline constexpr Clay_Color FORM_POPUP_COLOR = {250, 250, 250, 255};
inline constexpr Clay_Color FORM_POPUP_BORDER_COLOR = {160, 160, 160, 255};
inline constexpr Clay_Color FORM_ACCENT_COLOR = {65, 105, 168, 255};
inline constexpr Clay_Color FORM_ACCENT_HOVER_COLOR = {50, 85, 140, 255};
inline constexpr Clay_Color FORM_BORDER_COLOR = {190, 190, 190, 255};
inline constexpr Clay_Color FORM_ERROR_COLOR = {200, 40, 40, 255};
inline constexpr Clay_Color FORM_MUTED_COLOR = {130, 130, 130, 255};
inline constexpr Clay_Color FORM_BUTTON_COLOR = {228, 228, 228, 255};
inline constexpr float GRAPH_MENU_WIDTH = 270;  // layout_graph_menu.h
inline constexpr float TABLE_MENU_WIDTH = 220;  // layout_table_menu.h

// Dragging a tab: its empty slot in the bar, and the see-through copy following the mouse.
inline constexpr Clay_Color TAB_DRAG_SLOT_COLOR = {235, 235, 235, 255};
inline constexpr Clay_Color TAB_DRAG_PREVIEW_COLOR = {250, 250, 250, 220};
inline constexpr Clay_Color TAB_DRAG_PREVIEW_BORDER = {120, 120, 120, 220};
// Height of the draggable bar at the top of the terminal.
inline constexpr float TERMINAL_RESIZE_HANDLE = 5;
inline constexpr int GUTTER_MIN_LINES = 20;
inline constexpr float EDITOR_PADDING = editor_style::PADDING;

// Math popup size. The item list scrolls (mouse wheel / trackpad) once it is
// taller than MATH_MENU_LIST_MAX_HEIGHT; the search bar stays fixed above it.
inline constexpr float MATH_MENU_WIDTH = 320;
inline constexpr float MATH_MENU_LIST_MAX_HEIGHT = 300;
inline constexpr float MATH_MENU_ITEM_HEIGHT = 34;
inline constexpr float MATH_MENU_CORNER_RADIUS = 12;
inline constexpr float MATH_MENU_SEARCH_HEIGHT = 34;
inline constexpr uint16_t MATH_MENU_FONT_SIZE = 20;
// Font size of the rendered node previews on the right of each math item.
inline constexpr float MATH_MENU_PREVIEW_FONT_SIZE = 18;
}  // namespace layout_internal
