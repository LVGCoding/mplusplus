// Public layout API (declared in layout.h).
// The pieces live in header-only layout_*.h files included below, so this
// stays the only translation unit and no build files need to change:
//   layout_common.h        shared includes
//   layout_style.h         colors and sizes
//   layout_state.h         UI state and request flags
//   layout_editor_view.h   EditorView for the active file
//   layout_icons.h         custom-drawn icons
//   layout_menu.h          menu bar and File dropdown
//   layout_tabs.h          file tab bar, switching/closing tabs
//   layout_panels.h        title bar, workspace, terminal
//   layout_unsaved_popup.h unsaved-changes modal
//   layout_actions.h       handling the requested actions

#include "layout_common.h"

#include "layout_actions.h"
#include "layout_editor_view.h"
#include "layout_menu.h"
#include "layout_panels.h"
#include "layout_state.h"
#include "layout_style.h"
#include "layout_tabs.h"
#include "layout_unsaved_popup.h"

using namespace layout_internal;

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
        LayoutTitleBar(activeFile);
        LayoutMenuBar();
        if (activeFile) {
            LayoutFileBar(editor);
        }
        if (fileMenuOpen) {
            LayoutFileDropdown(canSave);
        }
        LayoutWorkspace(activeFile);
        LayoutTerminal();
        if (unsavedChangesOpen && activeFile) {
            UnsavedChangesPopup(*activeFile);
        }
    }
}

void ProcessLayoutActions(Editor& editor) {
    if (unsavedChangesOpen) {
        ProcessUnsavedChangesAction(editor);
        return;
    }
    ProcessTabRequests(editor);
    if (closeRequested) {
        ProcessCloseRequest(editor);
        return;
    }

    // One error string is shared by the handlers, as before.
    std::string error;
    ProcessNewRequest(editor, error);
    ProcessSaveRequest(editor, error);
    ProcessDocsRequest(editor, error);
    ProcessOpenRequest(editor, error);
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
