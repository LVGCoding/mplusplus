#pragma once

#include <cstddef>
#include <deque>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

// Editable text with a cursor, selection, undo/redo and clipboard: everything
// about editing plain text that doesn't depend on where it is drawn.
//
// Shared by the plain-text editor (FileText) and the one-line text boxes in
// the UI (TextInput), so anything added here reaches both.
//
// Positions are byte offsets on UTF-8 character boundaries.
class TextBuffer {
public:
    // Width in pixels of a piece of one line, for keeping the column on Up/Down.
    using LineMeasure = std::function<float(std::string_view)>;
    // Which typed characters to accept; empty accepts all printable ones.
    using CharacterFilter = std::function<bool(char32_t)>;

    std::string text;

    // Where the cursor is.
    std::size_t cursorPos = 0;

    // The other end of the selection. When it equals cursorPos, nothing is selected.
    std::size_t selectionStart = 0;

    // One line only (text boxes): Enter, Up and Down are left for the owner to
    // use, and line breaks in pasted text become spaces.
    bool singleLine = false;

    TextBuffer() = default;
    explicit TextBuffer(bool singleLine) : singleLine(singleLine) {}
    virtual ~TextBuffer() = default;

    // Replace all the text, put the cursor at the start and forget the undo history.
    void Reset(std::string newText = {});

    // --- Keys and typing -----------------------------------------------------------

    // Handle a key (raylib KEY_*): Left/Right, Up/Down, Backspace, Delete,
    // Enter. Shift extends the selection. `measure` may be null (e.g. in
    // tests); then Up/Down keep the column in characters. Returns true if the
    // text, cursor or selection changed.
    bool PressKey(int key, bool shift, const LineMeasure* measure);

    // Type a character at the cursor, replacing the selection.
    bool TypeCharacter(char32_t codepoint);

    // Read this frame's keyboard from raylib and apply it: editing keys, typed
    // characters (filtered by `accept`) and the shortcuts select all,
    // undo/redo, cut, copy and paste (Command on macOS, Control elsewhere).
    // Returns true if the text, cursor or selection changed.
    bool HandleKeyboard(const LineMeasure* measure, const CharacterFilter& accept = {});

    // --- Selection and clipboard ----------------------------------------------------

    bool HasSelection() const { return selectionStart != cursorPos; }
    void SelectAll();
    std::string SelectedText() const;
    // Delete the selection. Returns false if nothing was selected.
    bool DeleteSelection();
    // Insert text at the cursor, replacing the selection. Returns false for empty text.
    bool PasteText(std::string pasted);

    // --- Undo and redo ----------------------------------------------------------------
    //
    // Typing and deleting characters group into one step per word; anything
    // else (Enter, deleting a selection, paste) is a step of its own.

    bool Undo();
    bool Redo();
    bool CanUndo() const { return !undo_.empty(); }
    bool CanRedo() const { return !redo_.empty(); }

protected:
    // Called after every change to `text` (including undo and redo).
    virtual void TextChanged() {}

private:
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

    // Pixel (or character) column kept across consecutive Up/Down presses, so
    // the cursor returns to it after passing through shorter lines. Only valid
    // while the cursor is still at goalPos_, where the last Up/Down left it.
    std::optional<float> goalX_;
    std::size_t goalPos_ = 0;

    std::deque<UndoStep> undo_;
    std::deque<UndoStep> redo_;
    // True while the next typed/deleted character may join undo_.back().
    bool groupOpen_ = false;

    // The one way the text changes: replace [start, end) with `inserted`, put
    // the cursor after it, and record an undo step.
    void Replace(std::size_t start, std::size_t end, std::string inserted, EditKind kind,
                 bool endsGroup = false);
    bool MoveVertically(bool down, bool shift, const LineMeasure* measure);
    // Keep the cursor and selection inside the text (callers may set them directly).
    void ClampPositions();
};
