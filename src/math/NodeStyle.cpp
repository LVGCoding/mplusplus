#include "NodeStyle.h"

#include "ui/clay_renderer_raylib.h"
#include "ui/font.h"

#include "raylib.h"

#include <cstdint>

namespace node_style {

namespace {

const Color drawColor = {static_cast<unsigned char>(textColor.r),
                         static_cast<unsigned char>(textColor.g),
                         static_cast<unsigned char>(textColor.b),
                         static_cast<unsigned char>(textColor.a)};

// Custom draw callbacks only receive their bounds, so the stroke thickness is
// recovered from the element's width (which is fixed relative to font size).
float ThicknessFromWidth(float width, float widthScale) {
    return RuleThickness(width / widthScale);
}

// The surd: a short tick, a stroke down to the bottom, then a long stroke up to
// the top-right corner where the overline over the argument begins.
void DrawRadical(Clay_BoundingBox b) {
    const float t = ThicknessFromWidth(b.width, radicalWidthScale);
    const Vector2 a = {b.x, b.y + b.height * 0.62f};
    const Vector2 tick = {b.x + b.width * 0.2f, b.y + b.height * 0.55f};
    const Vector2 bottom = {b.x + b.width * 0.5f, b.y + b.height - t * 0.5f};
    const Vector2 top = {b.x + b.width, b.y + t * 0.5f};
    DrawLineEx(a, tick, t, drawColor);
    DrawLineEx(tick, bottom, t * 1.6f, drawColor);
    DrawLineEx(bottom, top, t, drawColor);
}

// Draw a delimiter described for the left side; `right` mirrors it.
template <Delimiter Kind, bool Right>
void DrawDelimiter(Clay_BoundingBox b) {
    const float t = ThicknessFromWidth(b.width, delimiterWidthScale);
    // Point at fractions (fx, fy) of the box, mirrored for the right side.
    auto p = [&](float fx, float fy) -> Vector2 {
        return {b.x + b.width * (Right ? 1.0f - fx : fx), b.y + b.height * fy};
    };
    auto line = [&](Vector2 from, Vector2 to) { DrawLineEx(from, to, t, drawColor); };

    if constexpr (Kind == Delimiter::Paren) {
        // Control point placed so the curve's midpoint sits at fx = 0.2.
        DrawSplineSegmentBezierQuadratic(p(0.8f, 0.0f), p(-0.4f, 0.5f), p(0.8f, 1.0f), t,
                                         drawColor);
    } else if constexpr (Kind == Delimiter::Bracket) {
        line(p(0.75f, 0.0f), p(0.3f, 0.0f));
        line(p(0.3f, 0.0f), p(0.3f, 1.0f));
        line(p(0.3f, 1.0f), p(0.75f, 1.0f));
    } else if constexpr (Kind == Delimiter::Brace) {
        line(p(0.85f, 0.0f), p(0.5f, 0.06f));
        line(p(0.5f, 0.06f), p(0.5f, 0.44f));
        line(p(0.5f, 0.44f), p(0.15f, 0.5f));
        line(p(0.15f, 0.5f), p(0.5f, 0.56f));
        line(p(0.5f, 0.56f), p(0.5f, 0.94f));
        line(p(0.5f, 0.94f), p(0.85f, 1.0f));
    } else if constexpr (Kind == Delimiter::Bar) {
        line(p(0.5f, 0.0f), p(0.5f, 1.0f));
    } else if constexpr (Kind == Delimiter::DoubleBar) {
        line(p(0.35f, 0.0f), p(0.35f, 1.0f));
        line(p(0.65f, 0.0f), p(0.65f, 1.0f));
    }
}

// Clay keeps a pointer to the custom data until rendering, so these must be
// static.
const ClayRaylibCustomDraw radicalDraw = {DrawRadical};

template <Delimiter Kind, bool Right>
const ClayRaylibCustomDraw delimiterDraw = {DrawDelimiter<Kind, Right>};

template <Delimiter Kind>
const ClayRaylibCustomDraw* DelimiterDraw(bool right) {
    return right ? &delimiterDraw<Kind, true> : &delimiterDraw<Kind, false>;
}

void StretchedSymbol(const ClayRaylibCustomDraw* draw, float width) {
    CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_FIXED(width), CLAY_SIZING_GROW(0)}},
                  .custom = {.customData = const_cast<ClayRaylibCustomDraw*>(draw)}}) {}
}

} // namespace

void Text(std::string_view text, float fontSize) {
    if (text.empty()) {
        return;
    }
    const Clay_String string = {.isStaticallyAllocated = false,
                                .length = static_cast<int32_t>(text.size()),
                                .chars = text.data()};
    CLAY_TEXT(string, {.textColor = textColor,
                       .fontId = FONT_BODY,
                       .fontSize = static_cast<uint16_t>(fontSize),
                       .wrapMode = CLAY_TEXT_WRAP_NONE});
}

void Placeholder(float fontSize) {
    CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_FIXED(std::round(fontSize * 0.6f)),
                                        CLAY_SIZING_FIXED(std::round(fontSize))}},
                  .border = {.color = placeholderColor, .width = CLAY_BORDER_OUTSIDE(1)}}) {}
}

void Radical(float fontSize) {
    StretchedSymbol(&radicalDraw, std::round(fontSize * radicalWidthScale));
}

void DelimiterSymbol(Delimiter kind, bool right, float fontSize) {
    const ClayRaylibCustomDraw* draw = nullptr;
    switch (kind) {
    case Delimiter::Paren:
        draw = DelimiterDraw<Delimiter::Paren>(right);
        break;
    case Delimiter::Bracket:
        draw = DelimiterDraw<Delimiter::Bracket>(right);
        break;
    case Delimiter::Brace:
        draw = DelimiterDraw<Delimiter::Brace>(right);
        break;
    case Delimiter::Bar:
        draw = DelimiterDraw<Delimiter::Bar>(right);
        break;
    case Delimiter::DoubleBar:
        draw = DelimiterDraw<Delimiter::DoubleBar>(right);
        break;
    }
    StretchedSymbol(draw, std::round(fontSize * delimiterWidthScale));
}

} // namespace node_style
