#pragma once

// Handlers for the requests recorded while building the layout.
// Called from ProcessLayoutActions after rendering, in the original order.

#include "layout_common.h"
#include "layout_state.h"
#include "layout_tabs.h"

namespace layout_internal {
    inline void ProcessHistoryRequests(Editor& editor) {
        FileBase* file = editor.ActiveFile();
        if (undoRequested) {
            undoRequested = false;
            if (file && file->CanUndo()) historyActivity |= file->Undo();
        }
        if (redoRequested) {
            redoRequested = false;
            if (file && file->CanRedo()) historyActivity |= file->Redo();
        }
    }

    // Edit -> Cut / Copy / Paste.
    inline void ProcessClipboardRequests(Editor& editor) {
        FileBase* file = editor.ActiveFile();
        if (cutRequested) {
            cutRequested = false;
            if (file) historyActivity |= file->CutToClipboard();
        }
        if (copyRequested) {
            copyRequested = false;
            if (file) file->CopyToClipboard();
        }
        if (pasteRequested) {
            pasteRequested = false;
            if (file) historyActivity |= file->PasteFromClipboard();
        }
    }

    inline bool SaveActiveFile(Editor& editor, std::string& error) {
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

    // Apply the button the user picked in the unsaved-changes popup (if any).
    inline void ProcessUnsavedChangesAction(Editor& editor) {
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
    }

    inline void ProcessTabRequests(Editor& editor) {
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
    }

    // Close the active tab, or ask first if it has unsaved changes.
    inline void ProcessCloseRequest(Editor& editor) {
        closeRequested = false;
        if (FileBase* file = editor.ActiveFile()) {
            if (file->modified) {
                unsavedChangesOpen = true;
                CloseAllMenus();
                closeError.clear();
            } else {
                CloseActiveTab(editor);
            }
        }
    }

    inline void ProcessNewRequest(Editor& editor, std::string& error) {
        if (!newRequested) return;
        newRequested = false;
        if (!editor.NewTemporaryFile(error)) {
            std::fprintf(stderr, "%s\n", error.c_str());
        }
    }

    inline void ProcessSaveRequest(Editor& editor, std::string& error) {
        if (!saveRequested) return;
        saveRequested = false;
        SaveActiveFile(editor, error);
        if (!error.empty()) {
            std::fprintf(stderr, "%s\n", error.c_str());
        }
    }

    inline void ProcessDocsRequest(Editor& editor, std::string& error) {
        if (!docsRequested) return;
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

    inline void ProcessOpenRequest(Editor& editor, std::string& error) {
        if (!openRequested) return;
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
