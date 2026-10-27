#include <cstdio>
#include <cstdlib>
#include <string>

#include "clay.h"
#include "defer.h"
#include "raylib.h"
#include "ui/clay_renderer_raylib.h"

// Font Table
enum FontId : uint16_t { 
  FONT_BODY = 0,
  FONT_BOLD = 1, 
  FONT_COUNT 
};

// Clay error handling
static void HandleClayError(Clay_ErrorData error) {
  std::fprintf(stderr, "Clay Error: %.*s\n", 
    error.errorText.length,
    error.errorText.chars);
}

// Main app layout
static void Layout(const std::string& EditorText) {
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

// Load the font table with selected fonts
static void LoadFontTable(Font fonts[]){
  fonts[FONT_BODY] = LoadFontEx("assets/font/LiberationSans-Regular.ttf", 32, nullptr, 0);
  fonts[FONT_BOLD] = LoadFontEx("assets/font/LiberationSans-Bold.ttf", 32, nullptr, 0);
}

int main(int argc, char* argv[]) {
  // Open the application window with set parameters
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT |
                 FLAG_WINDOW_HIGHDPI);
  InitWindow(1280, 720, "M++ Editor");
  defer(CloseWindow());

  // Load the font tables
  Font fonts[FONT_COUNT];
  LoadFontTable(fonts);

  // Allocate Clay's memory arena, deferring deallocation until main terminates
  uint32_t memorySize = Clay_MinMemorySize();
  void *memory = std::malloc(memorySize);
  defer(std::free(memory));
  Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(memorySize, memory);
  Clay_Initialize(arena, {(float)GetScreenWidth(), (float)GetScreenHeight()},
                  {HandleClayError, nullptr});
  Clay_SetMeasureTextFunction(ClayRaylib_MeasureText, fonts);

  // Active debug mode if ran with the debug flag
  if (argc > 1 && 
    (std::strcmp(argv[1], "--debug") == 0 || 
     std::strcmp(argv[1], "--d") == 0)) {
    Clay_SetDebugModeEnabled(true);
  }

  std::string EditorText;

  // Main render loop
  while (!WindowShouldClose()) {
    Clay_SetLayoutDimensions(
        {(float)GetScreenWidth(), (float)GetScreenHeight()});
    Vector2 mouse = GetMousePosition();
    Clay_SetPointerState({mouse.x, mouse.y},
                         IsMouseButtonDown(MOUSE_LEFT_BUTTON));
    Vector2 wheel = GetMouseWheelMoveV();
    Clay_UpdateScrollContainers(true, {wheel.x, wheel.y}, GetFrameTime());

    // Primitive user input capture
    int key = GetCharPressed();
    while (key > 0) {
        if (key >= 32 && key <= 126) {
            EditorText += static_cast<char>(key);
        }
        key = GetCharPressed();
    }

    // Handle deleting characters
    if (IsKeyPressed(KEY_BACKSPACE) && !EditorText.empty()) {
        EditorText.pop_back();
    }

    Clay_BeginLayout();
    Layout(EditorText);
    Clay_RenderCommandArray commands = Clay_EndLayout(GetFrameTime());

    BeginDrawing();
    ClearBackground(BLACK);
    ClayRaylib_Render(commands, fonts);
    EndDrawing();
  }
  return 0;
}
