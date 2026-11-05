#include "clay_renderer_raylib.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace {

Color overlayColor = {0, 0, 0, 0};

// Applies the active overlay (from OVERLAY_COLOR_START) by blending toward it.
Color ToRaylib(Clay_Color c) {
  Color out = {(unsigned char)std::lround(c.r), (unsigned char)std::lround(c.g),
               (unsigned char)std::lround(c.b),
               (unsigned char)std::lround(c.a)};
  if (overlayColor.a > 0) {
    float t = overlayColor.a / 255.0f;
    out.r = (unsigned char)(out.r + (overlayColor.r - out.r) * t);
    out.g = (unsigned char)(out.g + (overlayColor.g - out.g) * t);
    out.b = (unsigned char)(out.b + (overlayColor.b - out.b) * t);
  }
  return out;
}

// raylib expresses roundness as a fraction of the shorter side.
float Roundness(float radius, Rectangle r) {
  float shortest = std::min(r.width, r.height);
  return shortest > 0 ? std::clamp(radius * 2.0f / shortest, 0.0f, 1.0f) : 0;
}

void DrawBorder(Clay_BoundingBox b, const Clay_BorderRenderData &border) {
  Color color = ToRaylib(border.color);
  const Clay_CornerRadius &r = border.cornerRadius;
  const Clay_BorderWidth &w = border.width;

  if (w.left > 0)
    DrawRectangleRec({b.x, b.y + r.topLeft, (float)w.left,
                      b.height - r.topLeft - r.bottomLeft},
                     color);
  if (w.right > 0)
    DrawRectangleRec({b.x + b.width - w.right, b.y + r.topRight,
                      (float)w.right, b.height - r.topRight - r.bottomRight},
                     color);
  if (w.top > 0)
    DrawRectangleRec({b.x + r.topLeft, b.y,
                      b.width - r.topLeft - r.topRight, (float)w.top},
                     color);
  if (w.bottom > 0)
    DrawRectangleRec({b.x + r.bottomLeft, b.y + b.height - w.bottom,
                      b.width - r.bottomLeft - r.bottomRight, (float)w.bottom},
                     color);

  auto corner = [&](float radius, float cx, float cy, float start,
                    uint16_t width) {
    if (radius > 0)
      DrawRing({cx, cy}, radius - width, radius, start, start + 90, 10, color);
  };
  corner(r.topLeft, b.x + r.topLeft, b.y + r.topLeft, 180, w.top);
  corner(r.topRight, b.x + b.width - r.topRight, b.y + r.topRight, 270, w.top);
  corner(r.bottomLeft, b.x + r.bottomLeft, b.y + b.height - r.bottomLeft, 90,
         w.bottom);
  corner(r.bottomRight, b.x + b.width - r.bottomRight,
         b.y + b.height - r.bottomRight, 0, w.bottom);
}

} // namespace

Clay_Dimensions ClayRaylib_MeasureText(Clay_StringSlice text,
                                       Clay_TextElementConfig *config,
                                       void *userData) {
  Font font = ((Font *)userData)[config->fontId];
  if (!font.glyphs)
    font = GetFontDefault();
  float scale = (float)config->fontSize / font.baseSize;

  float lineWidth = 0, maxWidth = 0;
  int lines = 1;
  for (int i = 0; i < text.length;) {
    int bytes = 0;
    int codepoint = GetCodepointNext(text.chars + i, &bytes);
    i += bytes;
    if (codepoint == '\n') {
      maxWidth = std::max(maxWidth, lineWidth);
      lineWidth = 0;
      lines++;
      continue;
    }
    int index = GetGlyphIndex(font, codepoint);
    float advance = font.glyphs[index].advanceX != 0
                        ? font.glyphs[index].advanceX
                        : font.recs[index].width + font.glyphs[index].offsetX;
    lineWidth += advance * scale + config->letterSpacing;
  }
  maxWidth = std::max(maxWidth, lineWidth);

  float lineHeight =
      config->lineHeight > 0 ? config->lineHeight : config->fontSize;
  return {maxWidth, lineHeight * lines};
}

void ClayRaylib_Render(Clay_RenderCommandArray commands, Font *fonts) {
  std::string text;

  for (int32_t i = 0; i < commands.length; i++) {
    Clay_RenderCommand *cmd = Clay_RenderCommandArray_Get(&commands, i);
    Clay_BoundingBox b = cmd->boundingBox;
    Rectangle rect = {b.x, b.y, b.width, b.height};

    switch (cmd->commandType) {
    case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
      const auto &data = cmd->renderData.rectangle;
      Color color = ToRaylib(data.backgroundColor);
      if (data.cornerRadius.topLeft > 0)
        DrawRectangleRounded(rect, Roundness(data.cornerRadius.topLeft, rect),
                             8, color);
      else
        DrawRectangleRec(rect, color);
      break;
    }
    case CLAY_RENDER_COMMAND_TYPE_BORDER:
      DrawBorder(b, cmd->renderData.border);
      break;
    case CLAY_RENDER_COMMAND_TYPE_TEXT: {
      const auto &data = cmd->renderData.text;
      Font font = fonts[data.fontId];
      if (!font.glyphs)
        font = GetFontDefault();
      // raylib needs a null-terminated string; clay hands us a slice.
      text.assign(data.stringContents.chars, data.stringContents.length);
      DrawTextEx(font, text.c_str(), {b.x, b.y}, data.fontSize,
                 data.letterSpacing, ToRaylib(data.textColor));
      break;
    }
    case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
      const auto &data = cmd->renderData.image;
      Texture2D texture = *(Texture2D *)data.imageData;
      Color tint = data.backgroundColor.a > 0 ? ToRaylib(data.backgroundColor)
                                              : WHITE;
      DrawTexturePro(texture,
                     {0, 0, (float)texture.width, (float)texture.height}, rect,
                     {0, 0}, 0, tint);
      break;
    }
    case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
      BeginScissorMode((int)std::round(b.x), (int)std::round(b.y),
                       (int)std::round(b.width), (int)std::round(b.height));
      break;
    case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
      EndScissorMode();
      break;
    case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_START:
      overlayColor = ToRaylib(cmd->renderData.overlayColor.color);
      break;
    case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_END:
      overlayColor = {0, 0, 0, 0};
      break;
    case CLAY_RENDER_COMMAND_TYPE_CUSTOM: {
      const auto* custom = static_cast<const ClayRaylib_CustomDraw*>(
          cmd->renderData.custom.customData);
      if (custom && custom->draw)
        custom->draw(b);
      break;
    }
    case CLAY_RENDER_COMMAND_TYPE_NONE:
      break;
    }
  }
}
