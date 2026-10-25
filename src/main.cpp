#include <cstdio>
#include <cstdlib>

#include "clay.h"
#include "defer.h"
#include "raylib.h"
#include "ui/clay_renderer_raylib.h"

enum FontId : uint16_t { FONT_BODY = 0, FONT_COUNT };

static void HandleClayError(Clay_ErrorData error) {
  std::fprintf(stderr, "clay: %.*s\n", error.errorText.length,
               error.errorText.chars);
}

static void Layout() {
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
                {.textColor = text, .fontId = FONT_BODY, .fontSize = 32});
    }
    CLAY(CLAY_ID("Editor"),
         {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                     .padding = CLAY_PADDING_ALL(16)},
          .backgroundColor = panel,
          .cornerRadius = CLAY_CORNER_RADIUS(8)}) {
      CLAY_TEXT(CLAY_STRING("Hello from raylib + clay."),
                {.textColor = text, .fontId = FONT_BODY, .fontSize = 20});
    }
  }
}

int main() {
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT |
                 FLAG_WINDOW_HIGHDPI);
  InitWindow(1280, 720, "M++ Editor");
  defer(CloseWindow());

  Font fonts[FONT_COUNT];
  fonts[FONT_BODY] = GetFontDefault();

  uint32_t memorySize = Clay_MinMemorySize();
  void *memory = std::malloc(memorySize);
  defer(std::free(memory));
  Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(memorySize, memory);
  Clay_Initialize(arena, {(float)GetScreenWidth(), (float)GetScreenHeight()},
                  {HandleClayError, nullptr});
  Clay_SetMeasureTextFunction(ClayRaylib_MeasureText, fonts);

  while (!WindowShouldClose()) {
    if (IsKeyPressed(KEY_F12))
      Clay_SetDebugModeEnabled(!Clay_IsDebugModeEnabled());

    Clay_SetLayoutDimensions(
        {(float)GetScreenWidth(), (float)GetScreenHeight()});
    Vector2 mouse = GetMousePosition();
    Clay_SetPointerState({mouse.x, mouse.y},
                         IsMouseButtonDown(MOUSE_LEFT_BUTTON));
    Vector2 wheel = GetMouseWheelMoveV();
    Clay_UpdateScrollContainers(true, {wheel.x, wheel.y}, GetFrameTime());

    Clay_BeginLayout();
    Layout();
    Clay_RenderCommandArray commands = Clay_EndLayout(GetFrameTime());

    BeginDrawing();
    ClearBackground(BLACK);
    ClayRaylib_Render(commands, fonts);
    EndDrawing();
  }
  return 0;
}
