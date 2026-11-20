#include "NodeStyle.h"

#include "raylib.h"
#include "ui/clay_renderer_raylib.h"
#include "ui/font.h"

#include <cstdint>
#include <utility>

namespace node_style {

namespace {

constexpr Color DRAW_COLOR = {static_cast<unsigned char>(TEXT_COLOR.r),
                              static_cast<unsigned char>(TEXT_COLOR.g),
                              static_cast<unsigned char>(TEXT_COLOR.b),
                              static_cast<unsigned char>(TEXT_COLOR.a)};

// Custom draw callbacks only receive their bounds, so the stroke thickness is
// recovered from the element's width (which is fixed relative to font size).
float ThicknessFromWidth(float width,
                         float widthScale) {
    return RuleThickness(width / widthScale);
}

// The surd: a short tick, a stroke down to the bottom, then a long stroke up to
// the top-right corner where the overline over the argument begins.
void DrawRadical(Clay_BoundingBox bounds) {
    const float thickness = ThicknessFromWidth(bounds.width, RADICAL_WIDTH_SCALE);
    const Vector2 start = {bounds.x, bounds.y + bounds.height * 0.62f};
    const Vector2 tick = {bounds.x + bounds.width * 0.2f, bounds.y + bounds.height * 0.55f};
    const Vector2 bottom = {bounds.x + bounds.width * 0.5f,
                            bounds.y + bounds.height - thickness * 0.5f};
    const Vector2 top = {bounds.x + bounds.width, bounds.y + thickness * 0.5f};
    DrawLineEx(start, tick, thickness, DRAW_COLOR);
    DrawLineEx(tick, bottom, thickness * 1.6f, DRAW_COLOR);
    DrawLineEx(bottom, top, thickness, DRAW_COLOR);
}

// Draw a delimiter described for the left side; `right` mirrors it.
template <Delimiter KIND,
          bool RIGHT>
void DrawDelimiter(Clay_BoundingBox bounds) {
    const float thickness = ThicknessFromWidth(bounds.width, DELIMITER_WIDTH_SCALE);
    // Point at fractions (fractionX, fractionY) of the box, mirrored for the right side.
    auto pointAt = [&](float fractionX, float fractionY) -> Vector2 {
        return {bounds.x + bounds.width * (RIGHT ? 1.0f - fractionX : fractionX),
                bounds.y + bounds.height * fractionY};
    };
    auto drawLine = [&](Vector2 from, Vector2 to) { DrawLineEx(from, to, thickness, DRAW_COLOR); };

    if constexpr (KIND == Delimiter::PAREN) {
        // Control point placed so the curve's midpoint sits at fractionX = 0.2.
        DrawSplineSegmentBezierQuadratic(
            pointAt(0.8f, 0.0f), pointAt(-0.4f, 0.5f), pointAt(0.8f, 1.0f), thickness, DRAW_COLOR);
    } else if constexpr (KIND == Delimiter::BRACKET) {
        drawLine(pointAt(0.75f, 0.0f), pointAt(0.3f, 0.0f));
        drawLine(pointAt(0.3f, 0.0f), pointAt(0.3f, 1.0f));
        drawLine(pointAt(0.3f, 1.0f), pointAt(0.75f, 1.0f));
    } else if constexpr (KIND == Delimiter::BRACE) {
        drawLine(pointAt(0.85f, 0.0f), pointAt(0.5f, 0.06f));
        drawLine(pointAt(0.5f, 0.06f), pointAt(0.5f, 0.44f));
        drawLine(pointAt(0.5f, 0.44f), pointAt(0.15f, 0.5f));
        drawLine(pointAt(0.15f, 0.5f), pointAt(0.5f, 0.56f));
        drawLine(pointAt(0.5f, 0.56f), pointAt(0.5f, 0.94f));
        drawLine(pointAt(0.5f, 0.94f), pointAt(0.85f, 1.0f));
    } else if constexpr (KIND == Delimiter::BAR) {
        drawLine(pointAt(0.5f, 0.0f), pointAt(0.5f, 1.0f));
    } else if constexpr (KIND == Delimiter::DOUBLE_BAR) {
        drawLine(pointAt(0.35f, 0.0f), pointAt(0.35f, 1.0f));
        drawLine(pointAt(0.65f, 0.0f), pointAt(0.65f, 1.0f));
    }
}

// Clay keeps a pointer to the custom data until rendering, so these must be
// static.
const ClayRaylibCustomDraw RADICAL_DRAW = {DrawRadical};

template <Delimiter KIND, bool RIGHT>
const ClayRaylibCustomDraw DELIMITER_DRAW = {DrawDelimiter<KIND, RIGHT>};

template <Delimiter KIND>
const ClayRaylibCustomDraw* DelimiterDraw(bool right) {
    return right ? &DELIMITER_DRAW<KIND, true> : &DELIMITER_DRAW<KIND, false>;
}

void StretchedSymbol(const ClayRaylibCustomDraw* draw,
                     float width) {
    CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_FIXED(width), CLAY_SIZING_GROW(0)}},
                  .custom = {.customData = const_cast<ClayRaylibCustomDraw*>(draw)}}) {}
}

}  // namespace

void Text(std::string_view text,
          float fontSize) {
    if (text.empty()) {
        return;
    }
    const Clay_String string = {.isStaticallyAllocated = false,
                                .length = static_cast<int32_t>(text.size()),
                                .chars = text.data()};
    CLAY_TEXT(string,
              {.textColor = TEXT_COLOR,
               .fontId = FONT_BODY,
               .fontSize = static_cast<uint16_t>(fontSize),
               .wrapMode = CLAY_TEXT_WRAP_NONE});
}

void Placeholder(float fontSize) {
    CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_FIXED(std::round(fontSize * 0.6f)),
                                        CLAY_SIZING_FIXED(std::round(fontSize))}},
                  .border = {.color = PLACEHOLDER_COLOR, .width = CLAY_BORDER_OUTSIDE(1)}}) {}
}

void Radical(float fontSize) {
    StretchedSymbol(&RADICAL_DRAW, std::round(fontSize * RADICAL_WIDTH_SCALE));
}

void DelimiterSymbol(Delimiter kind,
                     bool right,
                     float fontSize) {
    const ClayRaylibCustomDraw* draw = nullptr;
    switch (kind) {
    case Delimiter::PAREN:
        draw = DelimiterDraw<Delimiter::PAREN>(right);
        break;
    case Delimiter::BRACKET:
        draw = DelimiterDraw<Delimiter::BRACKET>(right);
        break;
    case Delimiter::BRACE:
        draw = DelimiterDraw<Delimiter::BRACE>(right);
        break;
    case Delimiter::BAR:
        draw = DelimiterDraw<Delimiter::BAR>(right);
        break;
    case Delimiter::DOUBLE_BAR:
        draw = DelimiterDraw<Delimiter::DOUBLE_BAR>(right);
        break;
    }
    StretchedSymbol(draw, std::round(fontSize * DELIMITER_WIDTH_SCALE));
}


namespace {
std::optional<std::uint32_t> settingsRequest;
std::optional<ActionRequest> actionRequest;

constexpr Clay_Color BUTTON_COLOR = {236, 236, 236, 255};
constexpr Clay_Color BUTTON_HOVER_COLOR = {215, 215, 215, 255};
constexpr Clay_Color ACTION_HOVER_COLOR = {228, 228, 228, 255};

bool Clicked() {
    return Clay_Hovered() && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}
}  // namespace

void SettingsButton(std::uint32_t nodeId,
                    float fontSize) {
    const Clay_ElementId buttonId = CLAY_IDI("NodeSettingsButton", nodeId);
    CLAY(buttonId,
         {.layout = {.padding = {8, 8, 2, 2}},
          .backgroundColor = Clay_PointerOver(buttonId) ? BUTTON_HOVER_COLOR : BUTTON_COLOR,
          .cornerRadius = CLAY_CORNER_RADIUS(4)}) {
        Text("Settings", fontSize * 0.75f);
        if (Clicked()) {
            settingsRequest = nodeId;
        }
    }
}

std::optional<std::uint32_t> TakeSettingsRequest() {
    return std::exchange(settingsRequest, std::nullopt);
}

void ActionButton(std::uint32_t nodeId,
                  int action,
                  std::string_view label,
                  float fontSize,
                  Clay_SizingAxis width,
                  Clay_SizingAxis height) {
    const Clay_ElementId id = Clay__HashStringWithOffset(
        CLAY_STRING("NodeAction"), static_cast<uint32_t>(action), nodeId);
    CLAY(id,
         {.layout = {.sizing = {width, height},
                     .padding = {4, 4, 0, 0},
                     .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = Clay_PointerOver(id) ? ACTION_HOVER_COLOR : Clay_Color{0, 0, 0, 0},
          .cornerRadius = CLAY_CORNER_RADIUS(3)}) {
        const Clay_String text = {.isStaticallyAllocated = false,
                                  .length = static_cast<int32_t>(label.size()),
                                  .chars = label.data()};
        CLAY_TEXT(text,
                  {.textColor = MUTED_TEXT_COLOR,
                   .fontId = FONT_BODY,
                   .fontSize = static_cast<uint16_t>(std::max(MIN_FONT_SIZE, std::round(fontSize))),
                   .wrapMode = CLAY_TEXT_WRAP_NONE});
        if (Clicked()) {
            actionRequest = ActionRequest{nodeId, action};
        }
    }
}

std::optional<ActionRequest> TakeActionRequest() {
    return std::exchange(actionRequest, std::nullopt);
}

}  // namespace node_style
