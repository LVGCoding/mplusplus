#include "clay_renderer_raylib.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace {

Color overlayColor = {0, 0, 0, 0};

// Applies the active overlay (from OVERLAY_COLOR_START) by blending toward it.
Color ToRaylib(Clay_Color clayColor) {
    Color color = {static_cast<unsigned char>(std::lround(clayColor.r)),
                   static_cast<unsigned char>(std::lround(clayColor.g)),
                   static_cast<unsigned char>(std::lround(clayColor.b)),
                   static_cast<unsigned char>(std::lround(clayColor.a))};
    if (overlayColor.a > 0) {
        const float blendAmount = overlayColor.a / 255.0f;
        color.r = static_cast<unsigned char>(color.r + (overlayColor.r - color.r) * blendAmount);
        color.g = static_cast<unsigned char>(color.g + (overlayColor.g - color.g) * blendAmount);
        color.b = static_cast<unsigned char>(color.b + (overlayColor.b - color.b) * blendAmount);
    }
    return color;
}

// raylib expresses roundness as a fraction of the shorter side.
float Roundness(float radius,
                Rectangle rectangle) {
    float shortest = std::min(rectangle.width, rectangle.height);
    return shortest > 0 ? std::clamp(radius * 2.0f / shortest, 0.0f, 1.0f) : 0;
}

void DrawBorder(Clay_BoundingBox bounds,
                const Clay_BorderRenderData& border) {
    Color color = ToRaylib(border.color);
    const Clay_CornerRadius& corners = border.cornerRadius;
    const Clay_BorderWidth& widths = border.width;

    if (widths.left > 0) {
        DrawRectangleRec({bounds.x,
                          bounds.y + corners.topLeft,
                          static_cast<float>(widths.left),
                          bounds.height - corners.topLeft - corners.bottomLeft},
                         color);
    }
    if (widths.right > 0) {
        DrawRectangleRec({bounds.x + bounds.width - widths.right,
                          bounds.y + corners.topRight,
                          static_cast<float>(widths.right),
                          bounds.height - corners.topRight - corners.bottomRight},
                         color);
    }
    if (widths.top > 0) {
        DrawRectangleRec({bounds.x + corners.topLeft,
                          bounds.y,
                          bounds.width - corners.topLeft - corners.topRight,
                          static_cast<float>(widths.top)},
                         color);
    }
    if (widths.bottom > 0) {
        DrawRectangleRec({bounds.x + corners.bottomLeft,
                          bounds.y + bounds.height - widths.bottom,
                          bounds.width - corners.bottomLeft - corners.bottomRight,
                          static_cast<float>(widths.bottom)},
                         color);
    }

    auto corner = [&](float radius,
                      float centerX,
                      float centerY,
                      float startAngle,
                      uint16_t width) {
        if (radius > 0) {
            DrawRing(
                {centerX, centerY}, radius - width, radius, startAngle, startAngle + 90, 10, color);
        }
    };
    corner(
        corners.topLeft, bounds.x + corners.topLeft, bounds.y + corners.topLeft, 180, widths.top);
    corner(corners.topRight,
           bounds.x + bounds.width - corners.topRight,
           bounds.y + corners.topRight,
           270,
           widths.top);
    corner(corners.bottomLeft,
           bounds.x + corners.bottomLeft,
           bounds.y + bounds.height - corners.bottomLeft,
           90,
           widths.bottom);
    corner(corners.bottomRight,
           bounds.x + bounds.width - corners.bottomRight,
           bounds.y + bounds.height - corners.bottomRight,
           0,
           widths.bottom);
}

}  // namespace

Clay_Dimensions ClayRaylibMeasureText(Clay_StringSlice text,
                                      Clay_TextElementConfig* config,
                                      void* userData) {
    Font font = static_cast<Font*>(userData)[config->fontId];
    if (!font.glyphs) {
        font = GetFontDefault();
    }
    float scale = static_cast<float>(config->fontSize) / font.baseSize;

    float lineWidth = 0, maxWidth = 0;
    int lineCount = 1;
    // Advance by UTF-8 codepoints so measurement agrees with text rendering.
    for (int byteOffset = 0; byteOffset < text.length;) {
        int bytes = 0;
        int codepoint = GetCodepointNext(text.chars + byteOffset, &bytes);
        byteOffset += bytes;
        if (codepoint == '\n') {
            maxWidth = std::max(maxWidth, lineWidth);
            lineWidth = 0;
            lineCount++;
            continue;
        }
        int glyphIndex = GetGlyphIndex(font, codepoint);
        float advance = font.glyphs[glyphIndex].advanceX != 0
                            ? font.glyphs[glyphIndex].advanceX
                            : font.recs[glyphIndex].width + font.glyphs[glyphIndex].offsetX;
        lineWidth += advance * scale + config->letterSpacing;
    }
    maxWidth = std::max(maxWidth, lineWidth);

    float lineHeight = config->lineHeight > 0 ? config->lineHeight : config->fontSize;
    return {maxWidth, lineHeight * lineCount};
}

void ClayRaylibRender(Clay_RenderCommandArray commands,
                      Font* fonts) {
    // Reuse storage when converting Clay's text slices to null-terminated strings.
    std::string text;

    for (int32_t i = 0; i < commands.length; i++) {
        Clay_RenderCommand* command = Clay_RenderCommandArray_Get(&commands, i);
        Clay_BoundingBox bounds = command->boundingBox;
        Rectangle rectangle = {bounds.x, bounds.y, bounds.width, bounds.height};

        switch (command->commandType) {
        case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
            const auto& data = command->renderData.rectangle;
            Color color = ToRaylib(data.backgroundColor);
            if (data.cornerRadius.topLeft > 0) {
                DrawRectangleRounded(
                    rectangle, Roundness(data.cornerRadius.topLeft, rectangle), 8, color);
            } else {
                DrawRectangleRec(rectangle, color);
            }
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_BORDER:
            DrawBorder(bounds, command->renderData.border);
            break;
        case CLAY_RENDER_COMMAND_TYPE_TEXT: {
            const auto& data = command->renderData.text;
            Font font = fonts[data.fontId];
            if (!font.glyphs) {
                font = GetFontDefault();
            }
            // raylib needs a null-terminated string; clay hands us a slice.
            text.assign(data.stringContents.chars, data.stringContents.length);
            DrawTextEx(font,
                       text.c_str(),
                       {bounds.x, bounds.y},
                       data.fontSize,
                       data.letterSpacing,
                       ToRaylib(data.textColor));
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
            const auto& data = command->renderData.image;
            Texture2D texture = *static_cast<Texture2D*>(data.imageData);
            Color tint = data.backgroundColor.a > 0 ? ToRaylib(data.backgroundColor) : WHITE;
            DrawTexturePro(
                texture,
                {0, 0, static_cast<float>(texture.width), static_cast<float>(texture.height)},
                rectangle,
                {0, 0},
                0,
                tint);
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
            BeginScissorMode(static_cast<int>(std::round(bounds.x)),
                             static_cast<int>(std::round(bounds.y)),
                             static_cast<int>(std::round(bounds.width)),
                             static_cast<int>(std::round(bounds.height)));
            break;
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
            EndScissorMode();
            break;
        case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_START:
            overlayColor = ToRaylib(command->renderData.overlayColor.color);
            break;
        case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_END:
            overlayColor = {0, 0, 0, 0};
            break;
        case CLAY_RENDER_COMMAND_TYPE_CUSTOM: {
            const auto* custom =
                static_cast<const ClayRaylibCustomDraw*>(command->renderData.custom.customData);
            if (custom && custom->draw) {
                custom->draw(bounds);
            }
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_NONE:
            break;
        }
    }
}
