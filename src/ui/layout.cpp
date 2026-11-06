#include "layout.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "editor/editor.h"
#include "clay.h"
#include "raylib.h"
#include "file_dialog.h"
#include "font.h"
#include "clay_renderer_raylib.h"

namespace {
    bool newRequested = false;
    bool openRequested = false;
    bool docsRequested = false;
    bool saveRequested = false;
    bool closeRequested = false;
    bool fileMenuOpen = false;
    bool unsavedChangesOpen = false;
    enum class CloseAction { None, Save, Discard, Cancel };
    CloseAction closeAction = CloseAction::None;
    std::string closeError;

    constexpr Clay_Color menuText = {20, 20, 20, 255};
    constexpr Clay_Color menuHover = {205, 205, 205, 255};

    // Look sampled from UIMockup1.pptx
    constexpr Clay_Color titleBarColor = {78, 167, 46, 255};
    constexpr Clay_Color menuBarColor = {232, 232, 232, 255};
    constexpr Clay_Color dropdownColor = {209, 209, 209, 255};
    constexpr Clay_Color gutterColor = {232, 232, 232, 255};
    constexpr Clay_Color dividerColor = {209, 209, 209, 255};
    constexpr Clay_Color editorColor = {255, 255, 255, 255};
    constexpr uint16_t editorFontSize = 18;
    constexpr uint16_t editorLineHeight = 23;
    constexpr float gutterWidth = 46;
    constexpr Clay_Color terminalBorderColor = {174, 174, 174, 255};
    constexpr int gutterMinLines = 20;
    constexpr float editorPadding = 6;
    double lastCursorActivity = 0;

    float EditorTextWidth(const std::string& text, std::size_t start,
                          std::size_t end, Font font) {
        Clay_StringSlice slice = {.length = static_cast<int32_t>(end - start),
                                  .chars = text.c_str() + start, .baseChars = text.c_str()};
        Clay_TextElementConfig config = {.fontId = FONT_BODY, .fontSize = editorFontSize};
        return ClayRaylib_MeasureText(slice, &config, &font).width;
    }

    Vector2 EditorCursorPosition(const File& file, Font font) {
        const auto bounds = Clay_GetElementData(CLAY_ID("Editor")).boundingBox;
        const auto position = std::min(file.cursorPos, file.text.size());
        const auto newline = position == 0 ? std::string::npos : file.text.rfind('\n', position - 1);
        const auto start = newline == std::string::npos ? 0 : newline + 1;
        const auto row = std::count(file.text.begin(), file.text.begin() + position, '\n');
        return {bounds.x + editorPadding + EditorTextWidth(file.text, start, position, font),
                bounds.y + editorPadding + row * editorLineHeight};
    }

    Rectangle EditorViewport() {
        const auto workspace = Clay_GetElementData(CLAY_ID("Workspace"));
        const auto editor = Clay_GetElementData(CLAY_ID("Editor"));
        if (!workspace.found || !editor.found) return {};
        const float left = std::max(workspace.boundingBox.x, editor.boundingBox.x);
        const float right = std::min(workspace.boundingBox.x + workspace.boundingBox.width,
                                     editor.boundingBox.x + editor.boundingBox.width);
        return {left, workspace.boundingBox.y, std::max(0.0f, right - left),
                workspace.boundingBox.height};
    }

    void ClipEditorOverlay(Rectangle viewport) {
        BeginScissorMode(static_cast<int>(std::ceil(viewport.x)),
                         static_cast<int>(std::ceil(viewport.y)),
                         static_cast<int>(std::floor(viewport.width)),
                         static_cast<int>(std::floor(viewport.height)));
    }

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
              .border = {.color = menuText, .width = {.bottom = 2}}}) {
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
             {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(24)},
                         .padding = {10, 16, 0, 0},
                         .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
              .backgroundColor = enabled && Clay_PointerOver(id)
                  ? menuHover : Clay_Color{0, 0, 0, 0}}) {
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

void UpdateLayoutScroll(float wheelX, float wheelY, float deltaTime, bool textChanged) {
    constexpr float scrollIdleDelay = 0.15f;
    static float idleTime = scrollIdleDelay;
    static bool interrupted = false;

    // Trackpads deliver inertia as more wheel events, even with Clay drag scrolling disabled.
    if (wheelX != 0 || wheelY != 0) {
        idleTime = 0;
    } else {
        idleTime = std::min(idleTime + deltaTime, scrollIdleDelay);
    }
    if (textChanged && idleTime < scrollIdleDelay) {
        interrupted = true;
    }
    if (idleTime >= scrollIdleDelay) {
        interrupted = false;
    }
    const bool blocked = interrupted || IsLayoutModalOpen();
    Clay_UpdateScrollContainers(false,
        blocked ? Clay_Vector2{0, 0} : Clay_Vector2{wheelX, wheelY}, deltaTime);
}

bool ScrollEditorCursorIntoView(const File& file, Font* fonts) {
    auto scroll = Clay_GetScrollContainerData(CLAY_ID("Workspace"));
    const auto workspace = Clay_GetElementData(CLAY_ID("Workspace"));
    const auto editor = Clay_GetElementData(CLAY_ID("Editor"));
    if (!scroll.found || !workspace.found || !editor.found) {
        return false;
    }

    const Vector2 cursor = EditorCursorPosition(file, fonts[FONT_BODY]);
    const float viewportTop = workspace.boundingBox.y + editorPadding;
    const float viewportBottom = workspace.boundingBox.y + workspace.boundingBox.height - editorPadding;
    float offset = scroll.scrollPosition->y;
    if (cursor.y + editorLineHeight > viewportBottom) {
        offset -= cursor.y + editorLineHeight - viewportBottom;
    } else if (cursor.y < viewportTop) {
        offset += viewportTop - cursor.y;
    }
    const float minimumOffset = std::min(0.0f,
        scroll.scrollContainerDimensions.height - scroll.contentDimensions.height);
    offset = std::clamp(offset, minimumOffset, 0.0f);

    const float viewportLeft = workspace.boundingBox.x + editorPadding;
    const float viewportRight = workspace.boundingBox.x + workspace.boundingBox.width - editorPadding - 2;
    float horizontalOffset = scroll.scrollPosition->x;
    if (cursor.x > viewportRight) {
        horizontalOffset -= cursor.x - viewportRight;
    } else if (cursor.x < viewportLeft) {
        horizontalOffset += viewportLeft - cursor.x;
    }
    horizontalOffset = std::clamp(horizontalOffset,
        std::min(0.0f, scroll.scrollContainerDimensions.width - scroll.contentDimensions.width), 0.0f);
    if (offset == scroll.scrollPosition->y && horizontalOffset == scroll.scrollPosition->x) {
        return false;
    }
    scroll.scrollPosition->y = offset;
    scroll.scrollPosition->x = horizontalOffset;
    return true;
}

void Layout(Editor& editor) {
    static const std::string emptyText;
    const File* activeFile = editor.ActiveFile();
    if (!activeFile) ResetEditorCursorBlink();
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
          .backgroundColor = editorColor}) {
        // Green title bar from the mockup
        static std::string title;
        title = "M++ Editor";
        if (activeFile) {
            title += " - [";
            title += activeFile->path.empty() ? activeFile->TabName()
                                              : activeFile->path.string();
            title += "]";
        }
        CLAY(CLAY_ID("TitleBar"),
             {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(36)},
                         .padding = {8, 8, 0, 0}, .childGap = 10,
                         .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
              .backgroundColor = titleBarColor}) {
            CLAY_TEXT(CLAY_STRING("M"), {.textColor = menuText, .fontId = FONT_BOLD,
                                         .fontSize = 26, .wrapMode = CLAY_TEXT_WRAP_NONE});
            Clay_String titleText = {.isStaticallyAllocated = false,
                                     .length = static_cast<int32_t>(title.size()),
                                     .chars = title.c_str()};
            CLAY_TEXT(titleText, {.textColor = menuText, .fontId = FONT_BODY,
                                  .fontSize = 20, .wrapMode = CLAY_TEXT_WRAP_NONE});
        }
        CLAY(CLAY_ID("MenuBar"),
             {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(36)},
                         .padding = {4, 4, 0, 0}, .childGap = 0,
                         .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
              .backgroundColor = menuBarColor}) {
            const Clay_String labels[] = {CLAY_STRING("File"), CLAY_STRING("Edit"),
                                          CLAY_STRING("Insert"), CLAY_STRING("Run"),
                                          CLAY_STRING("Docs")};
            const Clay_ElementId ids[] = {CLAY_ID("FileMenu"), CLAY_ID("EditMenu"),
                                          CLAY_ID("InsertMenu"), CLAY_ID("RunMenu"),
                                          CLAY_ID("DocsMenu")};
            for (int i = 0; i < 5; ++i) {
                CLAY(ids[i],
                     {.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_GROW(0)},
                                 .padding = {8, 8, 0, 0}, .childGap = 6,
                                 .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
                      .backgroundColor = Clay_PointerOver(ids[i]) || (i == 0 && fileMenuOpen)
                          ? menuHover : Clay_Color{0, 0, 0, 0}}) {
                    CLAY_TEXT(labels[i], {.textColor = menuText,
                                          .fontId = FONT_BODY, .fontSize = 20,
                                          .wrapMode = CLAY_TEXT_WRAP_NONE});
                    if (false && i < 4) {  // mockup has no chevrons; change back to (i < 4) to show them
                        CLAY(CLAY_IDI("MenuChevron", i),
                             {.layout = {.sizing = {CLAY_SIZING_FIXED(12), CLAY_SIZING_FIXED(12)}},
                              .custom = {.customData = &chevron}}) {}
                    }
                    if (i == 0 && !unsavedChangesOpen && Clay_Hovered() &&
                        IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        fileMenuOpen = !fileMenuOpen;
                    }
                    if (i == 4 && !unsavedChangesOpen && Clay_Hovered() &&
                        IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        docsRequested = true;
                        fileMenuOpen = false;
                    }
                }
            }
        }
        if (activeFile) {
            CLAY(CLAY_ID("FileBar"),
                 {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(36)},
                             .padding = {static_cast<uint16_t>(gutterWidth), 24, 0, 0},
                             .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
                  .backgroundColor = editorColor,
                  .clip = {.horizontal = true},
                  .border = {.color = dividerColor, .width = {.bottom = 2}}}) {
                closeRequested = FileTab(*activeFile);
            }
        }
        if (fileMenuOpen) {
            CLAY(CLAY_ID("FileDropdown"),
                 {.layout = {.sizing = {CLAY_SIZING_FIXED(200), CLAY_SIZING_FIT(0)},
                             .padding = {0, 0, 4, 4},
                             .layoutDirection = CLAY_TOP_TO_BOTTOM},
                  .backgroundColor = dropdownColor,
                  .floating = {.offset = {0, 0}, .parentId = CLAY_ID("FileMenu").id,
                               .zIndex = 1,
                               .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_TOP,
                                                .parent = CLAY_ATTACH_POINT_LEFT_BOTTOM},
                               .attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID}}) {
                FileMenuItem(CLAY_ID("NewButton"), CLAY_STRING("New"), true, newRequested);
                FileMenuItem(CLAY_ID("OpenButton"), CLAY_STRING("Open File"), true, openRequested);
                FileMenuItem(CLAY_ID("SaveButton"), CLAY_STRING("Save File"), canSave, saveRequested);
            }
        }
        CLAY(CLAY_ID("Workspace"),
             {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}},
              // One offset keeps the gutter and editor text aligned while scrolling.
              .clip = {.horizontal = true, .vertical = true,
                       .childOffset = Clay_GetScrollOffset()}}) {
            // Line-number gutter from the mockup (same font size/line height as the editor text)
            static std::string lineNumbers;
            lineNumbers.clear();
            int lineCount = 1;
            for (char c : editorText) lineCount += c == '\n';
            if (lineCount < gutterMinLines) lineCount = gutterMinLines;
            for (int i = 1; i <= lineCount; ++i) {
                lineNumbers += std::to_string(i);
                if (i < lineCount) lineNumbers += '\n';
            }
            CLAY(CLAY_ID("Gutter"),
                 {.layout = {.sizing = {CLAY_SIZING_FIXED(gutterWidth), CLAY_SIZING_GROW(0)},
                             .padding = {10, 0, 6, 0}},
                  .backgroundColor = gutterColor}) {
                Clay_String numbersText = {.isStaticallyAllocated = false,
                                           .length = static_cast<int32_t>(lineNumbers.size()),
                                           .chars = lineNumbers.c_str()};
                if (activeFile) {  // only number lines when a file is open
                    CLAY_TEXT(numbersText, {.textColor = menuText, .fontId = FONT_BODY,
                                            .fontSize = editorFontSize, .lineHeight = editorLineHeight,
                                            .wrapMode = CLAY_TEXT_WRAP_NEWLINES});
                }
            }
            CLAY(CLAY_ID("Editor"),
                 {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                             .padding = {6, 6, 6, 6}},
                  .backgroundColor = editorColor}) {
                Clay_String clayText = {
                    .isStaticallyAllocated = false,
                    .length = static_cast<int32_t>(editorText.size()),
                    .chars = editorText.c_str()
                };
                CLAY_TEXT(clayText, {.textColor = menuText, .fontId = FONT_BODY,
                                     .fontSize = editorFontSize, .lineHeight = editorLineHeight,
                                     .wrapMode = CLAY_TEXT_WRAP_NEWLINES});
            }
        }
        // Terminal panel from the mockup (placeholder: no input/output wired up yet)
        CLAY(CLAY_ID("Terminal"),
             {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(140)},
                         .layoutDirection = CLAY_TOP_TO_BOTTOM},
              .backgroundColor = editorColor,
              .border = {.color = terminalBorderColor, .width = {.top = 2}}}) {
            CLAY(CLAY_ID("TerminalHeader"),
                 {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(30)},
                             .padding = {static_cast<uint16_t>(gutterWidth + 6), 8, 0, 0},
                             .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
                  .backgroundColor = menuBarColor}) {
                CLAY_TEXT(CLAY_STRING("Terminal"), {.textColor = menuText, .fontId = FONT_BODY,
                                                    .fontSize = 18,
                                                    .wrapMode = CLAY_TEXT_WRAP_NONE});
            }
            CLAY(CLAY_ID("TerminalBody"),
                 {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}}}) {
                CLAY(CLAY_ID("TerminalGutter"),
                     {.layout = {.sizing = {CLAY_SIZING_FIXED(gutterWidth), CLAY_SIZING_GROW(0)}},
                      .backgroundColor = gutterColor}) {}
                CLAY(CLAY_ID("TerminalOutput"),
                     {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                                 .padding = {6, 6, 6, 6}},
                      .clip = {.vertical = true}}) {
                    CLAY_TEXT(CLAY_STRING(">"), {.textColor = menuText, .fontId = FONT_BODY,
                                                 .fontSize = editorFontSize,
                                                 .lineHeight = editorLineHeight});
                }
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

    if (docsRequested) {
        docsRequested = false;
        std::filesystem::path path = "assets/doc/help.mpp";
        std::error_code filesystemError;
        if (!std::filesystem::exists(path, filesystemError)) {
            path = std::filesystem::path(GetApplicationDirectory()) / path;
        }
        if (!editor.OpenFile(path, error)) {
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

bool HandleEditorMouseInput(File& file, Font font) {
    static const File* draggingFile = nullptr;
    if (IsLayoutModalOpen() || fileMenuOpen || !IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        draggingFile = nullptr;
        return false;
    }
    const auto viewport = EditorViewport();
    const auto mouse = GetMousePosition();
    const bool clicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
        CheckCollisionPointRec(mouse, viewport);
    if (clicked) draggingFile = &file;
    if (draggingFile != &file) return false;

    const auto bounds = Clay_GetElementData(CLAY_ID("Editor")).boundingBox;
    const int row = std::max(0, static_cast<int>(std::floor(
        (mouse.y - bounds.y - editorPadding) / editorLineHeight)));
    std::size_t start = 0;
    for (int i = 0; i < row && start < file.text.size(); ++i) {
        const auto newline = file.text.find('\n', start);
        start = newline == std::string::npos ? file.text.size() : newline + 1;
    }
    const auto newline = file.text.find('\n', start);
    const auto end = newline == std::string::npos ? file.text.size() : newline;
    const float targetX = mouse.x - bounds.x - editorPadding;
    float x = 0;
    std::size_t position = start;
    while (position < end) {
        int bytes = 0;
        GetCodepointNext(file.text.c_str() + position, &bytes);
        const auto next = std::min(position + std::max(bytes, 1), end);
        const float width = EditorTextWidth(file.text, position, next, font);
        if (targetX < x + width / 2) break;
        x += width;
        position = next;
    }
    const auto previousPosition = file.cursorPos;
    const auto previousAnchor = file.selectionStart;
    file.cursorPos = position;
    if (clicked && !IsKeyDown(KEY_LEFT_SHIFT) && !IsKeyDown(KEY_RIGHT_SHIFT)) {
        file.selectionStart = position;
    }
    return clicked || previousPosition != file.cursorPos || previousAnchor != file.selectionStart;
}

void ResetEditorCursorBlink() {
    lastCursorActivity = GetTime();
}

void DrawEditorSelection(const File& file, Font font) {
    const auto start = std::min({file.selectionStart, file.cursorPos, file.text.size()});
    const auto end = std::min(std::max(file.selectionStart, file.cursorPos), file.text.size());
    if (start == end || IsLayoutModalOpen() || fileMenuOpen) return;
    const auto viewport = EditorViewport();
    if (viewport.width <= 0 || viewport.height <= 0) return;
    const auto bounds = Clay_GetElementData(CLAY_ID("Editor")).boundingBox;
    ClipEditorOverlay(viewport);
    std::size_t lineStart = 0;
    std::size_t row = 0;
    while (lineStart < end) {
        const auto newline = file.text.find('\n', lineStart);
        const auto lineEnd = newline == std::string::npos ? file.text.size() : newline;
        const auto from = std::max(start, lineStart);
        const auto to = std::min(end, lineEnd);
        const float y = bounds.y + editorPadding + row * editorLineHeight;
        if (from <= to && y + editorLineHeight > viewport.y && y < viewport.y + viewport.height) {
            const float x = bounds.x + editorPadding + EditorTextWidth(file.text, lineStart, from, font);
            float width = EditorTextWidth(file.text, from, to, font);
            // Give a selected newline (including blank lines) a visible highlight.
            if (newline != std::string::npos && end > newline) width += editorFontSize / 2.0f;
            if (width > 0) DrawRectangleRec({x, y, width, float(editorLineHeight)}, {65, 105, 168, 80});
        }
        if (newline == std::string::npos) break;
        lineStart = newline + 1;
        ++row;
    }
    EndScissorMode();
}

void DrawEditorCursor(const File& file, Font font) {
    if (IsLayoutModalOpen() || fileMenuOpen) return;
    const auto viewport = EditorViewport();
    if (viewport.width <= 0 || viewport.height <= 0) return;
    static const File* previousFile = nullptr;
    static std::filesystem::path previousPath;
    if (previousFile != &file || previousPath != file.path) {
        previousFile = &file;
        previousPath = file.path;
        ResetEditorCursorBlink();
    }
    const double elapsed = GetTime() - lastCursorActivity;
    if (elapsed >= 0.7 && static_cast<int>((elapsed - 0.7) / 0.5) % 2 == 1) return;
    const Vector2 cursor = EditorCursorPosition(file, font);
    ClipEditorOverlay(viewport);
    DrawRectangleRec({cursor.x, cursor.y, 2, float(editorLineHeight)}, {20, 20, 20, 255});
    EndScissorMode();
}
