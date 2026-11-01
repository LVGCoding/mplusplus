#include <cstdio>
#include <cstdlib>
#include <string>

#include "clay.h"
#include "defer.h"
#include "raylib.h"
#include "ui/clay_renderer_raylib.h"

#include "ui/font.h"
#include "editor/editor.h"
#include "ui/layout.h"
#include "editor/input.h"

// Clay error handling
static void HandleClayError(Clay_ErrorData error) {
  std::fprintf(stderr, "Clay Error: %.*s\n", 
    error.errorText.length,
    error.errorText.chars);
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
  defer(UnloadFontTable(fonts));

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

  Editor editor;
  editor.NewFile();

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
    if (File* file = editor.ActiveFile()) {
      HandleInput(file->text);
    }

    Clay_BeginLayout();
    Layout(editor);
    Clay_RenderCommandArray commands = Clay_EndLayout(GetFrameTime());

    BeginDrawing();
    ClearBackground(BLACK);
    ClayRaylib_Render(commands, fonts);
    EndDrawing();

    ProcessLayoutActions(editor);
  }
  return 0;
}
