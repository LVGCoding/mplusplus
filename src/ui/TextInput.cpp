#include "TextInput.h"

#include "ui/clay_renderer_raylib.h"

#include <algorithm>
#include <utility>

namespace {
// The cursor is drawn this much taller than the font size.
constexpr float cursorExtraHeight = 2.0f;
constexpr float cursorWidth = 2.0f;

std::optional<Clay_BoundingBox> LastBounds(Clay_ElementId id) {
    const Clay_ElementData data = Clay_GetElementData(id);
    return data.found ? std::optional(data.boundingBox) : std::nullopt;
}

Clay_String Slice(const std::string& text, std::size_t from, std::size_t to) {
    return {.isStaticallyAllocated = false,
            .length = static_cast<int32_t>(to - from),
            .chars = text.c_str() + from};
}
}  // namespace

TextInput::TextInput(std::string id) : id_(std::move(id)) {}

Clay_ElementId TextInput::PartId(Part part) const {
    const Clay_String name = {.isStaticallyAllocated = false,
                              .length = static_cast<int32_t>(id_.size()),
                              .chars = id_.c_str()};
    return Clay_GetElementIdWithIndex(name, part);
}

void TextInput::Clear() {
    buffer.Reset();
    scroll_ = 0.0f;
    Touch();
}

bool TextInput::HandleKeyboard(const TextBuffer::CharacterFilter& accept) {
    const std::string before = buffer.text;
    if (buffer.HandleKeyboard(nullptr, accept)) {
        Touch();
    }
    return buffer.text != before;
}

bool TextInput::HandleMouse(Font* fonts) {
    if (!IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        dragging_ = false;
        return false;
    }
    const auto box = LastBounds(PartId(Box));
    const auto row = LastBounds(PartId(Row));
    if (!box || !row || !fonts) {
        return false;
    }
    const Vector2 mouse = GetMousePosition();
    const bool clicked =
        IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
        CheckCollisionPointRec(mouse, {box->x, box->y, box->width, box->height});
    if (clicked) {
        dragging_ = true;
    }
    if (!dragging_) {
        return false;
    }

    // The character boundary nearest the mouse. The row's x already includes
    // the sideways scroll.
    const std::string& text = buffer.text;
    Clay_TextElementConfig config = {.fontId = fontId_, .fontSize = fontSize_};
    auto widthTo = [&](std::size_t end) {
        const Clay_StringSlice slice = {.length = static_cast<int32_t>(end),
                                        .chars = text.c_str(),
                                        .baseChars = text.c_str()};
        return ClayRaylib_MeasureText(slice, &config, fonts).width;
    };
    const float target = mouse.x - row->x;
    std::size_t position = 0;
    float x = 0.0f;
    while (position < text.size()) {
        int bytes = 0;
        GetCodepointNext(text.c_str() + position, &bytes);
        const std::size_t next = std::min(position + static_cast<std::size_t>(std::max(bytes, 1)),
                                          text.size());
        const float nextX = widthTo(next);
        if (target < (x + nextX) / 2) {
            break;
        }
        x = nextX;
        position = next;
    }

    const std::size_t previousCursor = buffer.cursorPos;
    const std::size_t previousAnchor = buffer.selectionStart;
    buffer.cursorPos = position;
    const bool extend = !clicked || IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    if (!extend) {
        buffer.selectionStart = position;
    }
    const bool moved =
        buffer.cursorPos != previousCursor || buffer.selectionStart != previousAnchor;
    if (moved || clicked) {
        Touch();
    }
    return moved;
}

// Uses last frame's positions: scroll just enough that the cursor is inside
// the box, and don't leave empty space at the right once the text fits.
void TextInput::ScrollCursorIntoView() {
    const auto box = LastBounds(PartId(Box));
    const auto row = LastBounds(PartId(Row));
    const auto cursor = LastBounds(PartId(CursorMarker));
    if (!box || !row || !cursor) {
        return;
    }
    const float visible = box->width - cursorWidth;
    const float cursorX = cursor->x - box->x;  // already includes scroll_
    if (cursorX < 0) {
        scroll_ -= cursorX;
    } else if (cursorX > visible) {
        scroll_ -= cursorX - visible;
    }
    scroll_ = std::clamp(scroll_, std::min(0.0f, visible - row->width), 0.0f);
}

void TextInput::Layout(const Style& style) {
    fontId_ = style.fontId;
    fontSize_ = style.fontSize;
    ScrollCursorIntoView();

    const std::string& text = buffer.text;
    const std::size_t cursor = std::min(buffer.cursorPos, text.size());
    const std::size_t anchor = std::min(buffer.selectionStart, text.size());
    const std::size_t selectedFrom = std::min(cursor, anchor);
    const std::size_t selectedTo = std::max(cursor, anchor);
    const float height = static_cast<float>(style.fontSize) + cursorExtraHeight;

    const double elapsed = GetTime() - lastActivity_;
    const bool cursorVisible =
        elapsed < 0.7 || static_cast<int>((elapsed - 0.7) / 0.5) % 2 == 0;

    const Clay_TextElementConfig textConfig = {.textColor = style.textColor,
                                               .fontId = style.fontId,
                                               .fontSize = style.fontSize,
                                               .wrapMode = CLAY_TEXT_WRAP_NONE};
    auto textPiece = [&](std::size_t from, std::size_t to) {
        if (from < to) {
            CLAY_TEXT(Slice(text, from, to), CLAY_TEXT_CONFIG(textConfig));
        }
    };
    // Takes no space, so moving the cursor never shifts the text; the bar
    // floats over the text from here.
    auto cursorMarker = [&] {
        CLAY(PartId(CursorMarker),
             {.layout = {.sizing = {CLAY_SIZING_FIXED(0), CLAY_SIZING_FIXED(height)}}}) {
            CLAY(PartId(CursorBar),
                 {.layout = {.sizing = {CLAY_SIZING_FIXED(cursorWidth), CLAY_SIZING_FIXED(height)}},
                  .backgroundColor = cursorVisible ? style.cursorColor : Clay_Color{0, 0, 0, 0},
                  .floating = {.offset = {-cursorWidth / 2, 0},
                               .zIndex = style.zIndex,
                               .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
                               .attachTo = CLAY_ATTACH_TO_PARENT,
                               .clipTo = CLAY_CLIP_TO_ATTACHED_PARENT}}) {}
        }
    };

    CLAY(PartId(Box), {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(height)},
                                  .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
                       .clip = {.horizontal = true, .childOffset = {scroll_, 0}}}) {
        CLAY(PartId(Row), {.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                                      .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}}}) {
            if (text.empty()) {
                cursorMarker();
                if (!style.placeholder.empty()) {
                    Clay_TextElementConfig placeholder = textConfig;
                    placeholder.textColor = style.placeholderColor;
                    CLAY_TEXT((Clay_String{.isStaticallyAllocated = false,
                                           .length = static_cast<int32_t>(style.placeholder.size()),
                                           .chars = style.placeholder.data()}),
                              CLAY_TEXT_CONFIG(placeholder));
                }
            } else {
                textPiece(0, selectedFrom);
                if (cursor == selectedFrom) {
                    cursorMarker();
                }
                if (selectedFrom < selectedTo) {
                    CLAY(PartId(Selected), {.layout = {.sizing = {CLAY_SIZING_FIT(0),
                                                                  CLAY_SIZING_FIT(0)}},
                                            .backgroundColor = style.selectionColor}) {
                        textPiece(selectedFrom, selectedTo);
                    }
                    if (cursor == selectedTo) {
                        cursorMarker();
                    }
                }
                textPiece(selectedTo, text.size());
            }
        }
    }
}
