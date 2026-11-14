#include "NodeStyle.h"

#include "raylib.h"
#include "ui/clay_renderer_raylib.h"
#include "ui/font.h"

#include <cstdint>

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

}  // namespace node_style
