#include "clay.h"
#include "defer.h"
#include "editor/editor.h"
#include "raylib.h"
#include "ui/clay_renderer_raylib.h"
#include "ui/font.h"
#include "ui/layout.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static void HandleClayError(Clay_ErrorData error) {
    std::fprintf(stderr, "Clay Error: %.*s\n", error.errorText.length, error.errorText.chars);
}

int main(int argc,
         char* argv[]) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT |
                   FLAG_WINDOW_HIGHDPI);
    InitWindow(1280, 720, "M++ Editor");
    DEFER(CloseWindow());
    // raylib quits on Escape by default. The editor uses Escape to close popups
    // (math menu, matrix insert wheel) and cancel deletes, so only the window's
    // close button quits.
    SetExitKey(KEY_NULL);

    Font fonts[FONT_COUNT];
    LoadFontTable(fonts);
    DEFER(UnloadFontTable(fonts));
    Vector2 fontDpi = GetWindowScaleDPI();

    // Allocate Clay's memory arena, deferring deallocation until main terminates
    uint32_t memorySize = Clay_MinMemorySize();
    void* memory = std::malloc(memorySize);
    DEFER(std::free(memory));
    Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(memorySize, memory);
    Clay_Initialize(arena,
                    {static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight())},
                    {HandleClayError, nullptr});
    Clay_SetMeasureTextFunction(ClayRaylibMeasureText, fonts);

    if (argc > 1 && (std::strcmp(argv[1], "--debug") == 0 || std::strcmp(argv[1], "--d") == 0)) {
        Clay_SetDebugModeEnabled(true);
    }

    Editor editor;

    while (!WindowShouldClose()) {

        const Vector2 currentDpi = GetWindowScaleDPI();
        if (currentDpi.x != fontDpi.x || currentDpi.y != fontDpi.y) {
            UnloadFontTable(fonts);
            LoadFontTable(fonts);
            // Cached widths came from the previous font atlas and must be measured again.
            Clay_ResetMeasureTextCache();
            fontDpi = currentDpi;
        }

        Clay_SetLayoutDimensions(
            {static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight())});
        Vector2 mouse = GetMousePosition();
        Clay_SetPointerState({mouse.x, mouse.y}, IsMouseButtonDown(MOUSE_LEFT_BUTTON));
        Vector2 wheel = GetMouseWheelMoveV();

        // The active file handles its own keyboard and mouse input.
        const bool cursorActivity = HandleActiveFileInput(editor, fonts);

        UpdateLayoutScroll(wheel.x, wheel.y, GetFrameTime(), cursorActivity);

        Clay_BeginLayout();
        Layout(editor);
        Clay_RenderCommandArray commands = Clay_EndLayout(GetFrameTime());

        if (cursorActivity && ScrollEditorCursorIntoView(editor, fonts)) {
            // Apply the new offset before drawing so typing is never rendered
            // off-screen.
            Clay_BeginLayout();
            Layout(editor);
            commands = Clay_EndLayout(GetFrameTime());
        }

        BeginDrawing();
        ClearBackground(BLACK);
        ClayRaylibRender(commands, fonts);

        // Cursor, selection, and anything else the active file draws over Clay
        DrawActiveFileOverlay(editor, fonts);

        EndDrawing();

        // File and tab changes can invalidate text borrowed by this frame's render commands.
        ProcessLayoutActions(editor);
    }
    return 0;
}
