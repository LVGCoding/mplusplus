#include "layout.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_map>

#include "editor/editor.h"
#include "clay.h"
#include "raylib.h"
#include "file_dialog.h"
#include "font.h"
#include "clay_renderer_raylib.h"
#include "EditorView.h"

namespace {
    bool newRequested = false;
    bool openRequested = false;
    bool docsRequested = false;
    bool saveRequested = false;
    bool closeRequested = false;
    // Tab index the user clicked to activate or close this frame (if any).
    std::size_t selectTabRequested = SIZE_MAX;
    std::size_t closeTabRequested = SIZE_MAX;
    // Remember each open file's editor scroll position so switching tabs restores it.
    std::unordered_map<const FileBase*, Clay_Vector2> tabScroll;
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
    constexpr uint16_t editorFontSize = editor_style::fontSize;
    constexpr uint16_t editorLineHeight = editor_style::lineHeight;
    constexpr float gutterWidth = 46;
    constexpr Clay_Color terminalBorderColor = {174, 174, 174, 255};
    constexpr int gutterMinLines = 20;
    constexpr float editorPadding = editor_style::padding;
    double lastCursorActivity = 0;

    void ResetEditorCursorBlink() {
        lastCursorActivity = GetTime();
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

    // Describe the editor area for the active file's rendering/input methods.
    EditorView MakeEditorView(Font* fonts) {
        EditorView view;
        view.fonts = fonts;
        const auto editor = Clay_GetElementData(CLAY_ID("Editor"));
        if (editor.found) {
            view.content = {editor.boundingBox.x, editor.boundingBox.y,
                            editor.boundingBox.width, editor.boundingBox.height};
        }
        view.viewport = EditorViewport();
        view.acceptsMouse = !fileMenuOpen;
        const double elapsed = GetTime() - lastCursorActivity;
        view.cursorVisible = elapsed < 0.7 || static_cast<int>((elapsed - 0.7) / 0.5) % 2 == 0;
        return view;
    }

    void DrawChevron(Clay_BoundingBox bounds) {
        const float x = bounds.x;
        const float y = bounds.y + bounds.height / 2;
        const Color color = {115, 115, 115, 255};
        DrawLineEx({x + 1, y - 3}, {x + 6, y + 2}, 1.5f, color);
        DrawLineEx({x + 6, y + 2}, {x + 11, y - 3}, 1.5f, color);
    }

    ClayRaylibCustomDraw chevron = {DrawChevron};

    void DrawCloseIcon(Clay_BoundingBox bounds) {
        const float x = bounds.x + bounds.width / 2;
        const float y = bounds.y + bounds.height / 2;
        DrawLineEx({x - 5, y - 5}, {x + 5, y + 5}, 1.5f, BLACK);
        DrawLineEx({x - 5, y + 5}, {x + 5, y - 5}, 1.5f, BLACK);
    }

    ClayRaylibCustomDraw closeIcon = {DrawCloseIcon};

    bool SaveActiveFile(Editor& editor, std::string& error) {
        FileBase* file = editor.ActiveFile();
        if (!file) {
            return false;
        }
        if (file->path.empty() || editor.IsTemporaryFile(*file)) {
            auto path = ShowSaveFileDialog(
                file->path.empty() ? std::filesystem::path("untitled.mpp")
                                   : file->path.filename(), error);
            return path && file->Save(*path, error);
        }
        return file->Save(file->path, error);
    }

    // Report tab clicks; switching/closing is deferred until after rendering.
    void FileTab(const FileBase& file, std::size_t index, bool active) {
        const std::string& name = file.TabName();
        const auto i = static_cast<uint32_t>(index);
        const Clay_ElementId tabId = CLAY_IDI("FileTab", i);
        const Clay_ElementId closeId = CLAY_IDI("FileTabCloseButton", i);
        const bool clickable = !unsavedChangesOpen && !fileMenuOpen;

        CLAY(tabId,
             {.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_GROW(0)},
                         .padding = {12, 4, 0, 0}, .childGap = 12,
                         .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
              .backgroundColor = !active && Clay_PointerOver(tabId)
                  ? Clay_Color{240, 240, 240, 255} : Clay_Color{0, 0, 0, 0},
              .border = {.color = active ? menuText : dividerColor,
                         .width = {.right = 1, .bottom = static_cast<uint16_t>(active ? 2 : 0)}}}) {
            Clay_String label = {
                .isStaticallyAllocated = false,
                .length = static_cast<int32_t>(name.size()),
                .chars = name.c_str()
            };
            CLAY_TEXT(label, {.textColor = active ? Clay_Color{20, 20, 20, 255}
                                                  : Clay_Color{100, 100, 100, 255},
                              .fontId = active ? FONT_BOLD : FONT_BODY, .fontSize = 18,
                              .wrapMode = CLAY_TEXT_WRAP_NONE});

            if (file.modified) {
                CLAY(CLAY_IDI("FileTabUnsavedDot", i),
                     {.layout = {.sizing = {CLAY_SIZING_FIXED(6), CLAY_SIZING_FIXED(6)}},
                      .backgroundColor = {20, 20, 20, 255},
                      .cornerRadius = CLAY_CORNER_RADIUS(3)}) {}
            }

            CLAY(closeId,
                 {.layout = {.sizing = {CLAY_SIZING_FIXED(28), CLAY_SIZING_FIXED(28)}},
                  .backgroundColor = Clay_PointerOver(closeId)
                      ? Clay_Color{190, 190, 190, 255} : Clay_Color{0, 0, 0, 0},
                  .cornerRadius = CLAY_CORNER_RADIUS(4)}) {
                if (clickable && Clay_Hovered() && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    closeTabRequested = index;
                }
                CLAY(CLAY_IDI("FileTabCloseIcon", i),
                     {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}},
                      .custom = {.customData = &closeIcon}}) {}
            }

            if (clickable && closeTabRequested != index && Clay_Hovered() &&
                IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                selectTabRequested = index;
            }
        }
    }

    // Save the current file's scroll offset and restore the newly active file's.
    void SwitchTab(Editor& editor, std::size_t index) {
        if (index == editor.ActiveTabIndex()) return;
        auto scroll = Clay_GetScrollContainerData(CLAY_ID("Workspace"));
        if (scroll.found) {
            if (const FileBase* current = editor.ActiveFile()) {
                tabScroll[current] = *scroll.scrollPosition;
            }
        }
        editor.SetActiveTab(index);
        if (scroll.found) {
            const auto saved = tabScroll.find(editor.ActiveFile());
            *scroll.scrollPosition = saved != tabScroll.end() ? saved->second : Clay_Vector2{0, 0};
        }
    }

    void CloseActiveTab(Editor& editor) {
        tabScroll.erase(editor.ActiveFile());
        editor.CloseActiveFile();
        // The next active file comes in with its own scroll position.
        auto scroll = Clay_GetScrollContainerData(CLAY_ID("Workspace"));
        if (scroll.found) {
            const auto saved = tabScroll.find(editor.ActiveFile());
            *scroll.scrollPosition = saved != tabScroll.end() ? saved->second : Clay_Vector2{0, 0};
        }
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

    void UnsavedChangesPopup(const FileBase& file) {
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

bool HandleActiveFileInput(Editor& editor, Font* fonts) {
    if (IsLayoutModalOpen()) {
        // Don't let characters typed over the popup reach the editor later.
        while (GetCharPressed() > 0) {}
        return false;
    }
    FileBase* file = editor.ActiveFile();
    if (!file) {
        return false;
    }
    const bool activity = file->HandleInput(MakeEditorView(fonts));
    if (activity) {
        ResetEditorCursorBlink();
    }
    return activity;
}

bool ScrollEditorCursorIntoView(Editor& editor, Font* fonts) {
    const FileBase* file = editor.ActiveFile();
    auto scroll = Clay_GetScrollContainerData(CLAY_ID("Workspace"));
    const auto workspace = Clay_GetElementData(CLAY_ID("Workspace"));
    if (!file || !scroll.found || !workspace.found) {
        return false;
    }

    const auto cursorBounds = file->CursorBounds(MakeEditorView(fonts));
    if (!cursorBounds) {
        return false;
    }
    const Rectangle cursor = *cursorBounds;
    const float viewportTop = workspace.boundingBox.y + editorPadding;
    const float viewportBottom = workspace.boundingBox.y + workspace.boundingBox.height - editorPadding;
    float offset = scroll.scrollPosition->y;
    if (cursor.y + cursor.height > viewportBottom) {
        offset -= cursor.y + cursor.height - viewportBottom;
    } else if (cursor.y < viewportTop) {
        offset += viewportTop - cursor.y;
    }
    const float minimumOffset = std::min(0.0f,
        scroll.scrollContainerDimensions.height - scroll.contentDimensions.height);
    offset = std::clamp(offset, minimumOffset, 0.0f);

    const float viewportLeft = workspace.boundingBox.x + editorPadding;
    const float viewportRight = workspace.boundingBox.x + workspace.boundingBox.width - editorPadding - 2;
    float horizontalOffset = scroll.scrollPosition->x;
    if (cursor.x + cursor.width > viewportRight) {
        horizontalOffset -= cursor.x + cursor.width - viewportRight;
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
    const FileBase* activeFile = editor.ActiveFile();
    if (!activeFile) ResetEditorCursorBlink();
    const bool canSave = activeFile && !activeFile->path.empty();
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
                  // Scrolls horizontally (trackpad / shift-wheel) when tabs overflow.
                  .clip = {.horizontal = true, .childOffset = Clay_GetScrollOffset()},
                  .border = {.color = dividerColor, .width = {.bottom = 2}}}) {
                const auto& files = editor.Files();
                for (std::size_t i = 0; i < files.size(); ++i) {
                    FileTab(*files[i], i, i == editor.ActiveTabIndex());
                }
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
            int lineCount = activeFile ? static_cast<int>(activeFile->LineCount()) : 1;
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
                if (activeFile) {
                    activeFile->LayoutContent();
                }
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
            CloseActiveTab(editor);
            unsavedChangesOpen = false;
            closeError.clear();
        }
        return;
    }
    if (closeTabRequested != SIZE_MAX) {
        // Closing a background tab brings it to the front first, so the
        // unsaved-changes popup and the close logic act on the right file.
        SwitchTab(editor, closeTabRequested);
        closeTabRequested = SIZE_MAX;
        selectTabRequested = SIZE_MAX;
        closeRequested = true;
    }
    if (selectTabRequested != SIZE_MAX) {
        SwitchTab(editor, selectTabRequested);
        selectTabRequested = SIZE_MAX;
    }

    if (closeRequested) {
        closeRequested = false;
        if (FileBase* file = editor.ActiveFile()) {
            if (file->modified) {
                unsavedChangesOpen = true;
                fileMenuOpen = false;
                closeError.clear();
            } else {
                CloseActiveTab(editor);
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

void DrawActiveFileOverlay(Editor& editor, Font* fonts) {
    if (IsLayoutModalOpen() || fileMenuOpen) return;
    const FileBase* file = editor.ActiveFile();
    if (!file) return;

    // Show the cursor solid for a moment after switching files.
    static const FileBase* previousFile = nullptr;
    static std::filesystem::path previousPath;
    if (previousFile != file || previousPath != file->path) {
        previousFile = file;
        previousPath = file->path;
        ResetEditorCursorBlink();
    }
    file->DrawOverlay(MakeEditorView(fonts));
}
