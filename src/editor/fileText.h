#pragma once

#include "fileBase.h"

#include <cstddef>
#include <deque>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

// A plain text file currently open in the M++ editor.
// Loading/saving lives in fileText.cpp; rendering and input in fileTextView.cpp.
struct FileText : FileBase {
    std::string text;

    // Position where the cursor is located in the text.
    std::size_t cursorPos = 0;

    // Starting position of the selected text.
    // When selectionStart == cursorPos, nothing is selected.
    std::size_t selectionStart = 0;

    // Width in pixels of a piece of one line, for keeping the column on Up/Down.
    using LineMeasure = std::function<float(std::string_view)>;

    bool Load(const std::filesystem::path& path, std::string& error) override;
    bool Save(const std::filesystem::path& path, std::string& error) override;

    // --- Editing (fileTextEdit.cpp) ----------------------------------------------

    // Handle a key (raylib KEY_*): arrows, Backspace, Delete, Enter. Shift
    // extends the selection. `measure` may be null (e.g. in tests); then Up/Down
    // keep the column in characters. Returns true if anything changed.
    bool PressKey(int key, bool shift, const LineMeasure* measure);

    // Type a character at the cursor, replacing the selection.
    bool TypeCharacter(char32_t codepoint);

    void SelectAll();

    // Undo and redo. Typing and deleting characters group into one step per
    // word; anything else (Enter, deleting a selection) is a step of its own.
    bool Undo();
    bool Redo();
    bool CanUndo() const { return !undo_.empty(); }
    bool CanRedo() const { return !redo_.empty(); }

    bool HandleInput(const EditorView& view) override;
    void LayoutContent() const override;
    std::size_t LineCount() const override;
    void DrawOverlay(const EditorView& view) const override;
    std::optional<Rectangle> CursorBounds(const EditorView& view) const override;

private:
    // True while a mouse drag that started in this file's editor is selecting text.
    bool dragging_ = false;

    // Pixel (or character) column kept across consecutive Up/Down presses, so
    // the cursor returns to it after passing through shorter lines. Only valid
    // while the cursor is still at goalPos_, where the last Up/Down left it.
    std::optional<float> goalX_;
    std::size_t goalPos_ = 0;

    enum class EditKind { Other, Typing, Deleting };

    // One undo step: at `position`, `removed` was replaced by `inserted`.
    struct UndoStep {
        std::size_t position = 0;
        std::string removed;
        std::string inserted;
        std::size_t cursorBefore = 0, anchorBefore = 0;
        std::size_t cursorAfter = 0, anchorAfter = 0;
        EditKind kind = EditKind::Other;
    };

    static constexpr std::size_t maxUndoSteps = 1000;

    std::deque<UndoStep> undo_;
    std::deque<UndoStep> redo_;
    // True while the next typed/deleted character may join undo_.back().
    bool groupOpen_ = false;
    // The text last loaded or saved; nullopt for a file never saved.
    std::optional<std::string> savedText_;

    // The one way the text changes: replace [start, end) with `inserted`, put
    // the cursor after it, and record an undo step.
    void Replace(std::size_t start, std::size_t end, std::string inserted, EditKind kind,
                 bool endsGroup = false);
    bool DeleteSelection();
    bool MoveVertically(bool down, bool shift, const LineMeasure* measure);
    void UpdateModified();

    // Apply this frame's typed characters and editing keys. Returns true if anything changed.
    bool HandleKeyboard(const EditorView& view);
    // Click/drag to place the cursor and select. Returns true if the cursor or selection moved.
    bool HandleMouse(const EditorView& view);
    void DrawSelection(const EditorView& view) const;
    void DrawCursor(const EditorView& view) const;
};

// Shared helpers for reading/writing a whole file as raw bytes.
bool ReadWholeFile(const std::filesystem::path& path, std::string& text, std::string& error);
bool WriteWholeFile(const std::filesystem::path& path, const std::string& text, std::string& error);
