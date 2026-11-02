#include "layout.h"

#include <cstdio>

#include "editor/editor.h"
#include "clay.h"
#include "raylib.h"
#include "file_dialog.h"
#include "font.h"

namespace {
bool openRequested = false;
bool saveRequested = false;
}

void Layout(Editor& editor){
    static const std::string emptyText;
    const File* activeFile = editor.ActiveFile();
    const bool canSave = activeFile && !activeFile->path.empty();
    const std::string& EditorText = activeFile ? activeFile->text : emptyText;
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
        CLAY(CLAY_ID("FileActions"),
             {.layout = {.childGap = 8,
                         .layoutDirection = CLAY_LEFT_TO_RIGHT}}) {
            CLAY(CLAY_ID("OpenButton"),
                 {.layout = {.padding = CLAY_PADDING_ALL(8)},
                  .backgroundColor = Clay_PointerOver(CLAY_ID("OpenButton"))
                      ? Clay_Color{80, 80, 94, 255}
                      : Clay_Color{65, 65, 75, 255},
                  .cornerRadius = CLAY_CORNER_RADIUS(4)}) {
                CLAY_TEXT(CLAY_STRING("Open File"),
                          {.textColor = text,
                           .fontId = FONT_BODY,
                           .fontSize = 20});

                if (Clay_Hovered() && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    openRequested = true;
                }
            }
            CLAY(CLAY_ID("SaveButton"),
                 {.layout = {.padding = CLAY_PADDING_ALL(8)},
                  .backgroundColor = !canSave
                      ? Clay_Color{52, 52, 62, 255}
                      : (Clay_PointerOver(CLAY_ID("SaveButton")) ? Clay_Color{80, 80, 94, 255}
                                        : Clay_Color{65, 65, 75, 255}),
                  .cornerRadius = CLAY_CORNER_RADIUS(4)}) {
                CLAY_TEXT(CLAY_STRING("Save File"),
                          {.textColor = canSave ? text : Clay_Color{125, 125, 140, 255},
                           .fontId = FONT_BODY,
                           .fontSize = 20});

                if (canSave && Clay_Hovered() &&
                    IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    saveRequested = true;
                }
            }
        }
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

void ProcessLayoutActions(Editor& editor) {
    std::string error;
    if (saveRequested) {
        saveRequested = false;
        if (File* file = editor.ActiveFile(); file && !file->path.empty()) {
            SaveFile(*file, file->path, error);
        }
        if (!error.empty()) {
            std::fprintf(stderr, "%s\n", error.c_str());
        }
    }

    if (openRequested) {
        openRequested = false;
        auto path = ShowOpenFileDialog(error);
        if (path) {
            editor.OpenFile(*path, error);
        }

        if (!error.empty()) {
            std::fprintf(stderr, "%s\n", error.c_str());
        }
    }
}
