#pragma once

// Settings popups for objects in the document (graph, table): the popup
// frame, where it goes, and the form pieces inside it (labelled rows, text
// and number fields, buttons).
//
// Fields are TextInputs, so they edit like the rest of the app (cursor keys,
// selection, undo, clipboard). Only the focused field shows a cursor and
// takes keys; the popup that owns the form passes its keys on with
// FormHandleKeys, which also moves focus on Tab and clicks.

#include "layout_common.h"
#include "layout_icons.h"
#include "layout_state.h"
#include "layout_style.h"
#include "ui/TextInput.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace layout_internal {

// One input box in a form. The id names its Clay elements and must be unique.
struct FormField {
    enum class Kind { TEXT, NUMBER };

    FormField(std::string id,
              Kind kind) : input(id), kind(kind), boxId_(id + "Box") {}

    TextInput input;
    Kind kind;
    // Shown with a red border until the field is edited again.
    bool invalid = false;

    const std::string& Text() const {
        return input.Text();
    }

    // Replace the text, with the cursor at the end.
    void SetText(const std::string& text) {
        input.Clear();
        input.buffer.Reset(text);
        input.buffer.cursorPos = input.buffer.selectionStart = text.size();
    }

    // The characters this field accepts. Text refuses braces: they would
    // unbalance the LaTeX that settings are stored in.
    TextBuffer::CharacterFilter Filter() const {
        if (kind == Kind::NUMBER) {
            return [](char32_t c) {
                return (c >= U'0' && c <= U'9') || c == U'.' || c == U'-' || c == U'+' ||
                       c == U'e' || c == U'E';
            };
        }
        return [](char32_t c) { return c != U'{' && c != U'}'; };
    }

    // The box around the input, for hit-testing clicks.
    Clay_ElementId BoxId() const {
        return Clay_GetElementId({.isStaticallyAllocated = false,
                                  .length = static_cast<int32_t>(boxId_.size()),
                                  .chars = boxId_.c_str()});
    }

private:
    std::string boxId_;
};

inline Clay_String FormString(std::string_view text) {
    return {.isStaticallyAllocated = false,
            .length = static_cast<int32_t>(text.size()),
            .chars = text.data()};
}

inline bool FormClicked() {
    return !unsavedChangesOpen && Clay_Hovered() && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

// --- Keyboard and focus (call before the layout) -------------------------------------

// Tab / Shift+Tab move focus through `fields` (in order, wrapping), and a
// click on a field focuses it; other keys and clicks go to the focused field.
// Enter and Escape are left to the caller. Returns true if the focused
// field's text changed.
inline bool FormHandleKeys(const std::vector<FormField*>& fields,
                           FormField*& focused,
                           Font* fonts) {
    if (fields.empty()) {
        return false;
    }
    // Uses last frame's layout, like the rest of the mouse handling.
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        for (FormField* field : fields) {
            if (Clay_PointerOver(field->BoxId())) {
                focused = field;
            }
        }
    }
    auto at = std::find(fields.begin(), fields.end(), focused);
    if (at == fields.end()) {
        at = fields.begin();
        focused = *at;
    }
    if (IsKeyPressed(KEY_TAB)) {
        const auto index = static_cast<std::size_t>(at - fields.begin());
        const bool back = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        focused = fields[(index + (back ? fields.size() - 1 : 1)) % fields.size()];
        focused->input.buffer.SelectAll();
        return false;
    }
    focused->input.HandleMouse(fonts);
    if (focused->input.HandleKeyboard(focused->Filter())) {
        focused->invalid = false;
        return true;
    }
    return false;
}

// --- Form pieces ---------------------------------------------------------------------

// Muted text that wraps to the form's width.
inline void FormHint(std::string_view text) {
    CLAY_TEXT(
        FormString(text),
        {.textColor = FORM_MUTED_COLOR, .fontId = FONT_BODY, .fontSize = FORM_HINT_FONT_SIZE});
}

inline void FormError(std::string_view text) {
    CLAY_TEXT(
        FormString(text),
        {.textColor = FORM_ERROR_COLOR, .fontId = FONT_BODY, .fontSize = FORM_HINT_FONT_SIZE});
}

// Small text between fields in a row, like "to" in "[-10] to [10]".
inline void FormJoiner(std::string_view text) {
    CLAY_TEXT(FormString(text),
              {.textColor = MENU_TEXT,
               .fontId = FONT_BODY,
               .fontSize = FORM_FONT_SIZE,
               .wrapMode = CLAY_TEXT_WRAP_NONE});
}

// A label on the left and the field(s) from `body` filling the rest.
template <typename Body>
void FormRow(std::string_view label,
             Body&& body) {
    CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                             .childGap = 8,
                             .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}}}) {
        CLAY_AUTO_ID(
            {.layout = {.sizing = {CLAY_SIZING_FIXED(FORM_LABEL_WIDTH), CLAY_SIZING_FIT(0)}}}) {
            CLAY_TEXT(FormString(label),
                      {.textColor = MENU_TEXT,
                       .fontId = FONT_BODY,
                       .fontSize = FORM_FONT_SIZE,
                       .wrapMode = CLAY_TEXT_WRAP_NONE});
        }
        body();
    }
}

// The input box. Only the focused field shows a cursor; the others show
// their text. `zIndex` is the popup's, plus one.
inline void FormInput(FormField& field,
                      bool focused,
                      int16_t zIndex) {
    const Clay_Color border = field.invalid ? FORM_ERROR_COLOR
                              : focused     ? FORM_ACCENT_COLOR
                                            : FORM_BORDER_COLOR;
    CLAY(field.BoxId(),
         {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(FORM_INPUT_HEIGHT)},
                     .padding = {8, 8, 0, 0},
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = {255, 255, 255, 255},
          .cornerRadius = CLAY_CORNER_RADIUS(4),
          .clip = {.horizontal = true},
          .border = {.color = border,
                     .width = CLAY_BORDER_OUTSIDE(static_cast<uint16_t>(focused ? 2 : 1))}}) {
        if (focused) {
            TextInput::Style style;
            style.fontSize = FORM_FONT_SIZE;
            style.textColor = MENU_TEXT;
            style.cursorColor = MENU_TEXT;
            style.zIndex = zIndex;
            field.input.Layout(style);
        } else {
            CLAY_TEXT(FormString(field.Text()),
                      {.textColor = MENU_TEXT,
                       .fontId = FONT_BODY,
                       .fontSize = FORM_FONT_SIZE,
                       .wrapMode = CLAY_TEXT_WRAP_NONE});
        }
    }
}

// A push button. Returns true on the frame it is clicked.
inline bool FormButton(Clay_ElementId id,
                       std::string_view label,
                       bool primary) {
    bool clicked = false;
    const bool hovered = Clay_PointerOver(id);
    const Clay_Color background = primary ? (hovered ? FORM_ACCENT_HOVER_COLOR : FORM_ACCENT_COLOR)
                                          : (hovered ? MENU_HOVER : FORM_BUTTON_COLOR);
    CLAY(id,
         {.layout = {.sizing = {CLAY_SIZING_FIT(56), CLAY_SIZING_FIXED(FORM_INPUT_HEIGHT)},
                     .padding = {10, 10, 0, 0},
                     .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = background,
          .cornerRadius = CLAY_CORNER_RADIUS(5)}) {
        CLAY_TEXT(FormString(label),
                  {.textColor = primary ? Clay_Color{255, 255, 255, 255} : MENU_TEXT,
                   .fontId = FONT_BODY,
                   .fontSize = FORM_FONT_SIZE,
                   .wrapMode = CLAY_TEXT_WRAP_NONE});
        clicked = FormClicked();
    }
    return clicked;
}

// Cancel and Apply, right-aligned. `prefix` makes the ids unique per popup.
inline void FormDialogButtons(uint32_t prefix,
                              bool& cancel,
                              bool& apply) {
    CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                             .childGap = 8,
                             .childAlignment = {.x = CLAY_ALIGN_X_RIGHT}}}) {
        if (FormButton(CLAY_IDI("FormCancel", prefix), "Cancel", false)) {
            cancel = true;
        }
        if (FormButton(CLAY_IDI("FormApply", prefix), "Apply", true)) {
            apply = true;
        }
    }
}

// --- The popup ------------------------------------------------------------------------

// The node `id` in the active M++ file if it is a `Node`, else null.
template <typename Node>
Node* FindActiveNode(Editor& editor,
                     std::uint32_t id) {
    auto* file = dynamic_cast<FileMpp*>(editor.ActiveFile());
    if (!file) {
        return nullptr;
    }
    const auto path = file->FindNode(id);
    return path ? dynamic_cast<Node*>(file->NodeAt(*path)) : nullptr;
}

// Where a popup goes this frame, and how tall it may be.
struct PopupPlacement {
    Clay_Vector2 position;
    float maxHeight;
};

// Beside the node `nodeId` (right, then left), level with its top; if neither
// side has room, below it, then above it. Kept inside the window. Nothing
// while the node has no box yet (just inserted): waits a couple of frames
// rather than jumping, then centres the popup. Call before the Root element
// is opened: once it is, its size reads as zero.
inline std::optional<PopupPlacement> PlacePopupNear(std::uint32_t nodeId,
                                                    Clay_ElementId popupId,
                                                    float width,
                                                    int& framesMissing) {
    const auto node = Clay_GetElementData(CLAY_IDI("EditorNode", nodeId));
    const auto root = Clay_GetElementData(CLAY_ID("Root"));
    const auto self = Clay_GetElementData(popupId);
    if (!root.found) {
        return std::nullopt;
    }
    constexpr float MARGIN = 8;
    constexpr float GAP = 8;
    const Clay_BoundingBox window = root.boundingBox;
    const float maxHeight = std::max(120.0f, window.height - 2 * MARGIN);
    // Until the popup has been laid out once its height is unknown, and a
    // guess can put it in the wrong place for a frame (a visible jump). So
    // its first frame is laid out off-screen, just to measure it.
    if (!self.found) {
        return PopupPlacement{{-10000, -10000}, maxHeight};
    }
    const float height = std::min(self.boundingBox.height, maxHeight);
    const float maxX = std::max(MARGIN, window.width - width - MARGIN);
    const float maxY = std::max(MARGIN, window.height - height - MARGIN);
    auto at = [&](float x, float y) { return PopupPlacement{{x, y}, maxHeight}; };

    if (!node.found) {
        if (++framesMissing < 3) {
            return std::nullopt;
        }
        return at(window.x + (window.width - width) / 2,
                  std::max(MARGIN, (window.height - height) / 2));
    }
    framesMissing = 0;
    const Clay_BoundingBox box = node.boundingBox;
    const float top = std::clamp(box.y, MARGIN, maxY);
    if (box.x + box.width + GAP + width <= window.width - MARGIN) {
        return at(box.x + box.width + GAP, top);
    }
    if (box.x - GAP - width >= MARGIN) {
        return at(box.x - GAP - width, top);
    }
    const float below = box.y + box.height + GAP;
    const float above = box.y - height - GAP;
    if (below + height <= window.height - MARGIN) {
        return at(std::clamp(box.x, MARGIN, maxX), below);
    }
    if (above >= MARGIN) {
        return at(std::clamp(box.x, MARGIN, maxX), above);
    }
    return at(std::clamp(box.x, MARGIN, maxX), top);
}

// A floating settings popup: a title with a close button, then `body`, which
// scrolls when the popup would be taller than the window. Sets `closeClicked`
// when the close button is clicked. Text inputs inside use zIndex + 1.
template <typename Body>
void FormPopup(Clay_ElementId popupId,
               std::string_view title,
               const PopupPlacement& placement,
               float width,
               int16_t zIndex,
               bool& closeClicked,
               Body&& body) {
    constexpr float HEADER_HEIGHT = 36;
    CLAY(popupId,
         {.layout = {.sizing = {CLAY_SIZING_FIXED(width), CLAY_SIZING_FIT(0, placement.maxHeight)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM},
          .backgroundColor = FORM_POPUP_COLOR,
          .cornerRadius = CLAY_CORNER_RADIUS(MATH_MENU_CORNER_RADIUS),
          .floating = {.offset = placement.position,
                       .zIndex = zIndex,
                       .attachTo = CLAY_ATTACH_TO_ROOT},
          .border = {.color = FORM_POPUP_BORDER_COLOR, .width = CLAY_BORDER_OUTSIDE(1)}}) {
        CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(HEADER_HEIGHT)},
                                 .padding = {12, 8, 0, 0},
                                 .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}}}) {
            CLAY_TEXT(FormString(title),
                      {.textColor = MENU_TEXT,
                       .fontId = FONT_BOLD,
                       .fontSize = FORM_TITLE_FONT_SIZE,
                       .wrapMode = CLAY_TEXT_WRAP_NONE});
            CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)}}}) {}
            const Clay_ElementId closeId = CLAY_IDI("FormPopupClose", popupId.id);
            CLAY(
                closeId,
                {.layout = {.sizing = {CLAY_SIZING_FIXED(20), CLAY_SIZING_FIXED(20)}},
                 .backgroundColor = Clay_PointerOver(closeId) ? MENU_HOVER : Clay_Color{0, 0, 0, 0},
                 .cornerRadius = CLAY_CORNER_RADIUS(4)}) {
                CLAY(CLAY_IDI("FormPopupCloseIcon", popupId.id),
                     {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}},
                      .custom = {.customData = &closeIcon}}) {}
                if (FormClicked()) {
                    closeClicked = true;
                }
            }
        }
        // Body: scrolls (mouse wheel) when it doesn't fit.
        const float bodyMaxHeight = std::max(0.0f, placement.maxHeight - HEADER_HEIGHT);
        CLAY(CLAY_IDI("FormPopupBody", popupId.id),
             {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0, bodyMaxHeight)},
                         .padding = {12, 12, 0, 12},
                         .childGap = 8,
                         .layoutDirection = CLAY_TOP_TO_BOTTOM},
              .clip = {.vertical = true, .childOffset = Clay_GetScrollOffset()}}) {
            body();
        }
    }
}
}  // namespace layout_internal
