#pragma once

#include "ui/EditorView.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

// The file base class for representing a file currently open in the M++ editor.
// FileText (plain text) and FileMpp (.mpp math documents) derive from this.
//
// Each file type owns how it is drawn and edited. Every frame, for the active file,
// the editor calls:
//   1. HandleInput()    - before layout: keyboard and mouse
//   2. LayoutContent()  - during the Clay layout, inside the "Editor" element
//   3. CursorBounds()   - after layout, to scroll the cursor into view
//   4. DrawOverlay()    - after Clay renders: cursor, selection, etc.
struct FileBase {
    std::filesystem::path path;
    bool modified = false;

    virtual ~FileBase() = default;

    // Read the file at `path` into this object. On success, `this->path` is set.
    virtual bool Load(const std::filesystem::path& path, std::string& error) = 0;

    // Write this object to `path`. On success, `this->path` is set.
    virtual bool Save(const std::filesystem::path& path, std::string& error) = 0;

    virtual const std::string& TabName() const;

    virtual bool Undo() { return false; }
    virtual bool Redo() { return false; }
    virtual bool CanUndo() const { return false; }
    virtual bool CanRedo() const { return false; }

    // --- Selection and clipboard ------------------------------------------------

    // The selected text (empty if nothing is selected), in the file's source form.
    virtual std::string SelectedText() const { return {}; }
    // Insert `text` at the cursor, replacing the selection, as one undo step.
    virtual bool PasteText(std::string /*text*/) { return false; }
    // Delete the selected range as one undo step. Returns false if nothing was selected.
    virtual bool DeleteSelection() { return false; }

    bool HasSelection() const { return !SelectedText().empty(); }

    // Shared by the keyboard shortcuts and the Edit menu. Each returns true if
    // the content changed (Copy never changes it, so it returns whether it copied).
    bool CopyToClipboard() const;
    bool CutToClipboard();
    bool PasteFromClipboard();

    // --- Rendering and input ------------------------------------------------

    // Handle this frame's keyboard and mouse input. Not called while a popup is
    // open. Set `modified` when the content changes. Returns true if the content
    // changed or the cursor/selection moved.
    virtual bool HandleInput(const EditorView& view) = 0;

    // Emit the Clay elements for the document body. Anything referenced by Clay
    // (e.g. string data) must stay alive until the frame has rendered.
    virtual void LayoutContent() const = 0;

    // Number of lines to show in the line-number gutter.
    virtual std::size_t LineCount() const = 0;

    // A rendered row to anchor its gutter label to. Zero uses fixed-height text lines.
    virtual uint32_t LineElementId(std::size_t /*line*/) const {
        return 0;
    }

    // Draw anything Clay can't, like the text cursor and selection highlight.
    // Called after Clay has rendered, with the same view as the layout.
    virtual void DrawOverlay(const EditorView& view) const = 0;

    // Screen rectangle of the text cursor, so the editor can scroll it into view.
    // Return std::nullopt if there is no cursor.
    virtual std::optional<Rectangle> CursorBounds(const EditorView& view) const = 0;

protected:
    // Keep the label alive until Clay finishes rendering the frame.
    mutable std::string tabName_;
};

// True if `path` should be opened as an M++ (.mpp) document.
bool IsMppPath(const std::filesystem::path& path);

// Create an empty file object of the right type for `path`:
// FileMpp for .mpp files, FileText for everything else.
std::unique_ptr<FileBase> CreateFileForPath(const std::filesystem::path& path);
