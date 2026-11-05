#include "layout.h"

#include <cstdio>

#include "editor/editor.h"
#include "clay.h"
#include "raylib.h"
#include "file_dialog.h"
#include "font.h"
#include "clay_renderer_raylib.h"

namespace {
    bool newRequested = false;
    bool openRequested = false;
    bool saveRequested = false;
    bool closeRequested = false;
    bool fileMenuOpen = false;
    bool unsavedChangesOpen = false;
    enum class CloseAction { None, Save, Discard, Cancel };
    CloseAction closeAction = CloseAction::None;
    std::string closeError;

    constexpr Clay_Color menuText = {20, 20, 20, 255};
    constexpr Clay_Color menuHover = {220, 220, 220, 255};

    void DrawChevron(Clay_BoundingBox bounds) {
        const float x = bounds.x;
        const float y = bounds.y + bounds.height / 2;
        const Color color = {115, 115, 115, 255};
        DrawLineEx({x + 1, y - 3}, {x + 6, y + 2}, 1.5f, color);
        DrawLineEx({x + 6, y + 2}, {x + 11, y - 3}, 1.5f, color);
    }

    ClayRaylib_CustomDraw chevron = {DrawChevron};

    void DrawCloseIcon(Clay_BoundingBox bounds) {
        const float x = bounds.x + bounds.width / 2;
        const float y = bounds.y + bounds.height / 2;
        DrawLineEx({x - 5, y - 5}, {x + 5, y + 5}, 1.5f, BLACK);
        DrawLineEx({x - 5, y + 5}, {x + 5, y - 5}, 1.5f, BLACK);
    }

    ClayRaylib_CustomDraw closeIcon = {DrawCloseIcon};

    bool SaveActiveFile(Editor& editor, std::string& error) {
        File* file = editor.ActiveFile();
        if (!file) {
            return false;
        }
        if (file->path.empty() || editor.IsTemporaryFile(*file)) {
            auto path = ShowSaveFileDialog(
                file->path.empty() ? std::filesystem::path("untitled.mpp")
                                   : file->path.filename(), error);
            return path && SaveFile(*file, *path, error);
        }
        return SaveFile(*file, file->path, error);
    }

    // Report close clicks; file mutation is deferred until after rendering.
    bool FileTab(const File& file) {
        const std::string& name = file.TabName();
        bool closeRequested = false;

        CLAY(CLAY_ID("ActiveFileTab"),
             {.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_GROW(0)},
                         .padding = {12, 4, 0, 0}, .childGap = 12,
                         .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
              .border = {.color = {0, 0, 0, 255}, .width = {.bottom = 1}}}) {
            Clay_String label = {
                .isStaticallyAllocated = false,
                .length = static_cast<int32_t>(name.size()),
                .chars = name.c_str()
            };
            CLAY_TEXT(label, {.textColor = {20, 20, 20, 255},
                              .fontId = FONT_BOLD, .fontSize = 18,
                              .wrapMode = CLAY_TEXT_WRAP_NONE});

            if (file.modified) {
                CLAY(CLAY_ID("FileTabUnsavedDot"),
                     {.layout = {.sizing = {CLAY_SIZING_FIXED(6), CLAY_SIZING_FIXED(6)}},
                      .backgroundColor = {20, 20, 20, 255},
                      .cornerRadius = CLAY_CORNER_RADIUS(3)}) {}
            }

            CLAY(CLAY_ID("FileTabCloseButton"),
                 {.layout = {.sizing = {CLAY_SIZING_FIXED(28), CLAY_SIZING_FIXED(28)}},
                  .backgroundColor = Clay_PointerOver(CLAY_ID("FileTabCloseButton"))
                      ? Clay_Color{190, 190, 190, 255} : Clay_Color{0, 0, 0, 0},
                  .cornerRadius = CLAY_CORNER_RADIUS(4)}) {
                closeRequested = !unsavedChangesOpen && Clay_Hovered() &&
                    IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
                CLAY(CLAY_ID("FileTabCloseIcon"),
                     {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}},
                      .custom = {.customData = &closeIcon}}) {}
            }
        }

        return closeRequested;
    }

    void FileMenuItem(Clay_ElementId id, Clay_String label, bool enabled,
                      bool& requested) {
        CLAY(id,
             {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(36)},
                         .padding = {12, 12, 0, 0},
                         .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
              .backgroundColor = enabled && Clay_PointerOver(id)
                  ? menuHover : Clay_Color{0, 0, 0, 0},
              .cornerRadius = CLAY_CORNER_RADIUS(4)}) {
            CLAY_TEXT(label, {.textColor = enabled ? menuText : Clay_Color{150, 150, 150, 255},
                              .fontId = FONT_BODY, .fontSize = 18});
            if (enabled && !unsavedChangesOpen && Clay_Hovered() &&
                IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                requested = true;
                fileMenuOpen = false;
            }
        }
    }

    void ClosePopupButton(Clay_ElementId id, Clay_String label, CloseAction action) {
        const bool discard = action == CloseAction::Discard;
        CLAY(id,
             {.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIXED(38)},
                         .padding = {16, 16, 0, 0},
                         .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
              .backgroundColor = Clay_PointerOver(id)
                  ? (discard ? Clay_Color{240, 200, 200, 255} : menuHover)
                  : (discard ? Clay_Color{250, 225, 225, 255} : Clay_Color{235, 235, 235, 255}),
              .cornerRadius = CLAY_CORNER_RADIUS(4),
              .border = {.color = {160, 160, 160, 255}, .width = CLAY_BORDER_ALL(1)}}) {
            CLAY_TEXT(label, {.textColor = menuText, .fontId = FONT_BODY, .fontSize = 18});
            if (Clay_Hovered() && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                closeAction = action;
            }
        }
    }

    void UnsavedChangesPopup(const File& file) {
        // Clay uses this message until rendering finishes.
        static std::string message;
        message = file.path.filename().string();
        if (message.empty()) message = "untitled.mpp";
        message += " has unsaved changes.";

        CLAY(CLAY_ID("UnsavedChangesOverlay"),
             {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                         .padding = CLAY_PADDING_ALL(24),
                         .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER}},
              .backgroundColor = {0, 0, 0, 120},
              .floating = {.zIndex = 10, .attachTo = CLAY_ATTACH_TO_ROOT}}) {
            CLAY(CLAY_ID("UnsavedChangesPopup"),
                 {.layout = {.sizing = {CLAY_SIZING_GROW(0, 480), CLAY_SIZING_FIT(0)},
                             .padding = CLAY_PADDING_ALL(24), .childGap = 20,
                             .layoutDirection = CLAY_TOP_TO_BOTTOM},
                  .backgroundColor = {250, 250, 250, 255},
                  .cornerRadius = CLAY_CORNER_RADIUS(8),
                  .border = {.color = {160, 160, 160, 255}, .width = CLAY_BORDER_ALL(1)}}) {
                Clay_String text = {.isStaticallyAllocated = false,
                                    .length = static_cast<int32_t>(message.size()),
                                    .chars = message.c_str()};
                CLAY_TEXT(text, {.textColor = menuText, .fontId = FONT_BODY, .fontSize = 20});
                if (!closeError.empty()) {
                    Clay_String error = {.isStaticallyAllocated = false,
                                         .length = static_cast<int32_t>(closeError.size()),
                                         .chars = closeError.c_str()};
                    CLAY_TEXT(error, {.textColor = {160, 30, 30, 255},
                                      .fontId = FONT_BODY, .fontSize = 16});
                }
                CLAY(CLAY_ID("UnsavedChangesButtons"),
                     {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                                 .childGap = 12,
                                 .childAlignment = {.x = CLAY_ALIGN_X_RIGHT}}}) {
                    ClosePopupButton(CLAY_ID("CancelCloseButton"),
                                     CLAY_STRING("Cancel"), CloseAction::Cancel);
                    CLAY(CLAY_ID("UnsavedChangesButtonSpacer"),
                         {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(1)}}}) {}
                    ClosePopupButton(CLAY_ID("SaveAndCloseButton"),
                                     CLAY_STRING("Save and Close"), CloseAction::Save);
                    ClosePopupButton(CLAY_ID("CloseAnywayButton"),
                                     CLAY_STRING("Close Anyway"), CloseAction::Discard);
                }
            }
        }
    }
}

bool IsLayoutModalOpen() {
    return unsavedChangesOpen;
}

void Layout(Editor& editor) {
    static const std::string emptyText;
    const File* activeFile = editor.ActiveFile();
    const bool canSave = activeFile && !activeFile->path.empty();
    const std::string& editorText = activeFile ? activeFile->text : emptyText;

    if (IsKeyPressed(KEY_ESCAPE) ||
        (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
         !Clay_PointerOver(CLAY_ID("FileMenu")) &&
         !Clay_PointerOver(CLAY_ID("FileDropdown")))) {
        fileMenuOpen = false;
    }

    CLAY(CLAY_ID("Root"),
         {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM},
          .backgroundColor = activeFile ? Clay_Color{30, 30, 36, 255}
                                        : Clay_Color{255, 255, 255, 255}}) {
        CLAY(CLAY_ID("MenuBar"),
             {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(54)},
                         .childGap = 4,
                         .childAlignment = {.x = CLAY_ALIGN_X_CENTER,
                                            .y = CLAY_ALIGN_Y_CENTER}},
              .backgroundColor = {238, 238, 238, 255}}) {
            const Clay_String labels[] = {CLAY_STRING("File"), CLAY_STRING("Edit"),
                                          CLAY_STRING("Insert"), CLAY_STRING("Run"),
                                          CLAY_STRING("Docs")};
            const Clay_ElementId ids[] = {CLAY_ID("FileMenu"), CLAY_ID("EditMenu"),
                                          CLAY_ID("InsertMenu"), CLAY_ID("RunMenu"),
                                          CLAY_ID("DocsMenu")};
            for (int i = 0; i < 5; ++i) {
                CLAY(ids[i],
                     {.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIXED(38)},
                                 .padding = {12, 12, 0, 0}, .childGap = 16,
                                 .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
                      .backgroundColor = Clay_PointerOver(ids[i]) || (i == 0 && fileMenuOpen)
                          ? menuHover : Clay_Color{0, 0, 0, 0},
                      .cornerRadius = CLAY_CORNER_RADIUS(4)}) {
                    CLAY_TEXT(labels[i], {.textColor = menuText,
                                          .fontId = FONT_BODY, .fontSize = 20,
                                          .wrapMode = CLAY_TEXT_WRAP_NONE});
                    if (i < 4) {
                        CLAY(CLAY_IDI("MenuChevron", i),
                             {.layout = {.sizing = {CLAY_SIZING_FIXED(12), CLAY_SIZING_FIXED(12)}},
                              .custom = {.customData = &chevron}}) {}
                    }
                    if (i == 0 && !unsavedChangesOpen && Clay_Hovered() &&
                        IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        fileMenuOpen = !fileMenuOpen;
                    }
                }
            }
        }
        if (activeFile) {
            CLAY(CLAY_ID("FileBar"),
                 {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(38)},
                             .padding = {24, 24, 0, 0},
                             .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
                  .backgroundColor = {210, 210, 210, 255},
                  .clip = {.horizontal = true}}) {
                closeRequested = FileTab(*activeFile);
            }
        }
        if (fileMenuOpen) {
            CLAY(CLAY_ID("FileDropdown"),
                 {.layout = {.sizing = {CLAY_SIZING_FIXED(180), CLAY_SIZING_FIT(0)},
                             .padding = CLAY_PADDING_ALL(4),
                             .layoutDirection = CLAY_TOP_TO_BOTTOM},
                  .backgroundColor = {250, 250, 250, 255},
                  .cornerRadius = CLAY_CORNER_RADIUS(4),
                  .floating = {.offset = {0, 8}, .parentId = CLAY_ID("FileMenu").id,
                               .zIndex = 1,
                               .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_TOP,
                                                .parent = CLAY_ATTACH_POINT_LEFT_BOTTOM},
                               .attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID},
                  .border = {.color = {205, 205, 205, 255}, .width = CLAY_BORDER_ALL(1)}}) {
                FileMenuItem(CLAY_ID("NewButton"), CLAY_STRING("New"), true, newRequested);
                FileMenuItem(CLAY_ID("OpenButton"), CLAY_STRING("Open File"), true, openRequested);
                FileMenuItem(CLAY_ID("SaveButton"), CLAY_STRING("Save File"), canSave, saveRequested);
            }
        }
        CLAY(CLAY_ID("Workspace"),
             {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                         .padding = CLAY_PADDING_ALL(16)}}) {
            CLAY(CLAY_ID("Editor"),
                 {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                             .padding = CLAY_PADDING_ALL(16)},
                  .backgroundColor = activeFile ? Clay_Color{45, 45, 54, 255}
                                                : Clay_Color{255, 255, 255, 255},
                  .cornerRadius = CLAY_CORNER_RADIUS(8)}) {
                Clay_String clayText = {
                    .isStaticallyAllocated = false,
                    .length = static_cast<int32_t>(editorText.size()),
                    .chars = editorText.c_str()
                };
                CLAY_TEXT(clayText, {.textColor = {230, 230, 235, 255},
                                     .fontId = FONT_BODY, .fontSize = 20});
            }
        }
        if (unsavedChangesOpen && activeFile) {
            UnsavedChangesPopup(*activeFile);
        }
    }
}

void ProcessLayoutActions(Editor& editor) {
    std::string error;
    if (unsavedChangesOpen) {
        const CloseAction action = closeAction;
        closeAction = CloseAction::None;
        if (action == CloseAction::Cancel) {
            unsavedChangesOpen = false;
            closeError.clear();
            return;
        }
        if (action == CloseAction::Discard ||
            (action == CloseAction::Save && SaveActiveFile(editor, closeError))) {
            editor.CloseActiveFile();
            unsavedChangesOpen = false;
            closeError.clear();
        }
        return;
    }
    if (closeRequested) {
        closeRequested = false;
        if (File* file = editor.ActiveFile()) {
            if (file->modified) {
                unsavedChangesOpen = true;
                fileMenuOpen = false;
                closeError.clear();
            } else {
                editor.CloseActiveFile();
            }
        }
        return;
    }

    if (newRequested) {
        newRequested = false;
        if (!editor.NewTemporaryFile(error)) {
            std::fprintf(stderr, "%s\n", error.c_str());
        }
    }

    if (saveRequested) {
        saveRequested = false;
        SaveActiveFile(editor, error);
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
