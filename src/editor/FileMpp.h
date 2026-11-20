#pragma once

#include "EditorSettings.h"
#include "fileBase.h"
#include "lang/Source.h"
#include "math/EditorNode.h"
#include "math/LineNode.h"

#include <cstddef>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class TextNode;

// An M++ (.mpp) document currently open in the M++ editor.
struct FileMpp : FileBase {
    // LaTeX/M++ source used for saving. Preserve the original bytes after loading;
    // after an edit, regenerate this from the node tree in UpdateSource().
    std::string source;

    // Parsed document: one LineNode per source line, each normalized (see
    // LineNode::Normalize). Built from `source` by SetSource().
    std::vector<std::unique_ptr<LineNode>> content;

    // Cursor position: [row, ...path inside that row's LineNode]. Always ends
    // at an offset in a TextNode (see CursorPath).
    CursorPath cursorPos = {0, 0, 0};

    // The other end of the selection (the anchor); cursorPos is the end that
    // moves. When selectionStart == cursorPos, nothing is selected.
    CursorPath selectionStart = {0, 0, 0};

    // The selection in document order, widened so both ends are in the same
    // LineNode: if one end is inside a fraction/root/matrix and the other
    // isn't, the whole object is selected. Ends in different rows are
    // widened to positions directly in those rows.
    struct Selection {
        CursorPath start;
        CursorPath end;
    };

    // What Backspace does next to a fraction/root/matrix. A setting later;
    // for now the default from EditorSettings.h.
    editor_settings::StructureBackspace backspaceMode = editor_settings::STRUCTURE_BACKSPACE;

    // Something marked in red, deleted by pressing the same key again.
    struct PendingDelete {
        CursorPath node;  // the node (WHOLE) or the matrix (ROW/COLUMN)
        DeleteHighlight::Part part = DeleteHighlight::Part::WHOLE;
        std::size_t row = 0;
        std::size_t column = 0;
        int key = 0;  // KEY_BACKSPACE or KEY_DELETE
    };

    // The wheel around a matrix cell for inserting a row or column.
    enum class WheelDirection { UP, DOWN, LEFT, RIGHT };
    struct InsertWheel {
        CursorPath matrix;
        std::size_t row = 0;
        std::size_t column = 0;
        std::optional<WheelDirection> direction;
    };

    // Problems found by checking or running the document (see src/lang).
    // Each is underlined in red, and hovering it shows the message. Cleared
    // whenever the content changes, since their positions would be stale.
    std::vector<lang::Diagnostic> diagnostics;

    // Re-check the document while it is edited. Set once it has been run, so
    // documents that are just notes don't fill up with red.
    bool checkWhileEditing = false;

    // Goes up with every change to the content (edits, undo, redo, loading).
    std::uint64_t ContentVersion() const {
        return contentVersion_;
    }

    FileMpp();

    bool Load(const std::filesystem::path& path,
              std::string& error) override;
    bool Save(const std::filesystem::path& path,
              std::string& error) override;

    // Parse `text`, replace the document, reset the cursor and clear undo history.
    // This does not set the saved snapshot or modified flag; Load() handles those.
    void SetSource(std::string text);

    // The document serialized back to M++ source.
    std::string ContentText() const;

    // --- Editing (fileMppEdit.cpp) -------------------------------------------------

    // Handle a key (raylib KEY_*): arrows, Backspace, Delete, Enter, Tab, Escape.
    // `measure` may be null (e.g. in tests); then Up/Down land at the start of
    // the target. Returns true if the cursor, content or highlights changed.
    bool PressKey(int key,
                  bool shift,
                  const TextMeasure* measure);

    // Type a character at the cursor. A backslash asks for the math popup
    // instead (see TakeMathMenuRequest). Returns true if anything changed.
    bool TypeCharacter(char32_t codepoint);

    // Insert a new node (e.g. an empty fraction) at the cursor and move into it.
    bool InsertNode(std::unique_ptr<EditorNode> node);

    // True once after a backslash was typed: the UI should open the math popup.
    bool TakeMathMenuRequest();

    // Place the cursor using screen coordinates from the latest layout.
    // Returns false if no rendered position is available.
    bool ClickAt(float x,
                 float y,
                 const TextMeasure* measure);

    // --- Selection (fileMppSelection.cpp) ----------------------------------------

    // Put the cursor at `path` with nothing selected.
    void MoveCursorTo(CursorPath path);

    // The selected range, or nothing when the selection is empty.
    std::optional<Selection> GetSelection() const;

    // Select the whole document.
    void SelectAll();

    // Delete the selected range. Returns false if nothing was selected.
    bool DeleteSelection() override;

    // Clipboard text uses M++/LaTeX source, preserving selected structures.
    std::string SelectedText() const override;
    // Parse and insert source, replacing the selection as one undo step.
    // Inside a math slot, top-level line breaks become spaces.
    bool PasteText(std::string text) override;

    // The cursor position nearest to a screen point, from the last layout.
    // Used by ClickAt and by drag-selecting. In fileMppView.cpp.
    std::optional<CursorPath> PositionAt(float x,
                                         float y,
                                         const TextMeasure* measure) const;

    // --- Undo and redo (fileMppHistory.cpp) ---------------------------------------
    //
    // Each step holds copies of only the rows an edit changed, so history stays
    // proportional to the affected rows. Contiguous typing groups through the next space;
    // consecutive character deletions in the same row can span word boundaries.

    bool Undo() override;
    bool Redo() override;
    bool CanUndo() const override {
        return !undo_.empty();
    }
    bool CanRedo() const override {
        return !redo_.empty();
    }

    const std::optional<PendingDelete>& GetPendingDelete() const {
        return pendingDelete_;
    }
    const std::optional<InsertWheel>& GetInsertWheel() const {
        return insertWheel_;
    }

    // --- Tree lookup -------------------------------------------------------------------

    // The node a path (without a trailing text offset) leads to, or null.
    EditorNode* NodeAt(PathView path) const;

    // Path to the node with this id (see EditorNode::GetId), or nothing if it
    // is not in the document. Ids survive undo and redo, unlike pointers.
    std::optional<CursorPath> FindNode(std::uint32_t id) const;

    // Change the node with this id as one undo step, e.g. a graph's settings
    // from its menu. `edit` returns false if it changed nothing. The cursor
    // moves into the node's row first (undo steps cover the cursor's rows).
    // Returns false if the node is gone or nothing changed.
    bool EditNode(std::uint32_t id,
                  const std::function<bool(EditorNode&)>& edit);

    // The node that `path` ends in and the offset into it, or null if the path
    // doesn't lead to a TextNode.
    TextNode* CursorText(const CursorPath& path,
                         std::size_t* offset = nullptr) const;

    // The innermost fraction/root/matrix holding the cursor (outlined in green).
    const EditorNode* ActiveStructure() const;

    // Rendering and input, in fileMppView.cpp.
    bool HandleInput(const EditorView& view) override;
    void LayoutContent() const override;
    std::size_t LineCount() const override;
    uint32_t LineElementId(std::size_t line) const override;
    void DrawOverlay(const EditorView& view) const override;
    std::optional<Rectangle> CursorBounds(const EditorView& view) const override;

private:
    // Line ending used to join rows back into source ("\n" or "\r\n").
    std::string lineEnding_ = "\n";

    // Screen x kept across consecutive Up/Down presses so the cursor returns to
    // the same column after passing through shorter rows.
    // Only valid while the cursor is still at goalPath_, where the last
    // vertical move left it.
    std::optional<float> goalX_;
    CursorPath goalPath_;

    std::optional<PendingDelete> pendingDelete_;
    std::uint64_t contentVersion_ = 0;

    // Enter with the cursor directly in `row`: split it, keeping the
    // indentation (one level more after a "{").
    void SplitRow(std::size_t row);
    // True if the cursor is between the quotes of a string in its line.
    bool InsideString() const;
    // The diagnostic under the mouse (from the last overlay drawn) and where
    // the mouse was, for its tooltip.
    mutable std::optional<std::size_t> hoveredDiagnostic_;
    mutable Vector2 hoverPoint_ = {};
    std::optional<InsertWheel> insertWheel_;
    bool mathMenuRequested_ = false;

    // True while a mouse drag that started in this file is selecting.
    bool dragging_ = false;

    // --- Undo history (fileMppHistory.cpp) ---

    // Edits that can be grouped with the previous step of the same kind.
    enum class EditKind { OTHER, TYPING, DELETING };

    struct UndoStep {
        std::size_t firstRow = 0;
        // Rows to put back at firstRow, replacing `replaces` rows that are there
        // now. Undo and redo swap them with the current rows.
        std::vector<std::unique_ptr<LineNode>> rows;
        std::size_t replaces = 0;
        CursorPath cursorBefore, anchorBefore;
        CursorPath cursorAfter, anchorAfter;
    };

    // Rows an edit in progress may change, copied before it starts: from the
    // row before the selection/cursor to the row after it.
    struct OpenEdit {
        std::size_t firstRow = 0;
        std::size_t rowCount = 0;  // content.size() at the start
        std::vector<std::unique_ptr<LineNode>> rows;
        std::vector<std::string> texts;
        CursorPath cursor, anchor;
        EditKind kind = EditKind::OTHER;
        bool endsGroup = false;
    };

    // Brackets one user action (a key, a typed character, an insert) so all
    // the changes it makes become a single undo step. Nested scopes share the outer
    // snapshot, so replacing a selection does not create an extra undo step.
    class EditScope {
    public:
        EditScope(FileMpp& file,
                  EditKind kind,
                  bool endsGroup = false)
            : file_(file) {
            file_.BeginEdit(kind, endsGroup);
        }
        ~EditScope() {
            file_.EndEdit();
        }
        EditScope(const EditScope&) = delete;
        EditScope& operator=(const EditScope&) = delete;

    private:
        FileMpp& file_;
    };

    static constexpr std::size_t MAX_UNDO_STEPS = 1000;

    std::deque<UndoStep> undo_;
    std::deque<UndoStep> redo_;
    std::optional<OpenEdit> openEdit_;
    int editDepth_ = 0;
    bool edited_ = false;
    // Kind of undo_.back() while the next edit may still join it.
    std::optional<EditKind> openGroup_;
    // The text last loaded or saved; nullopt for a file never saved.
    std::optional<std::string> savedSource_;

    void BeginEdit(EditKind kind,
                   bool endsGroup);
    void EndEdit();
    void UpdateSource();
    void ClearHistory();
    // Swap a step's rows with the rows they replace.
    void SwapRows(UndoStep& step);
    void AfterHistoryMove();

    std::optional<Clay_BoundingBox> CaretBox(const TextMeasure* measure) const;
    void DrawInsertWheel(const EditorView& view) const;
    void DrawSelection(const EditorView& view) const;
    void DrawDiagnostics(const EditorView& view) const;
    // Screen boxes covering a range, one per row it spans.
    std::vector<Rectangle> RangeBoxes(const CursorPath& start,
                                      const CursorPath& end,
                                      const TextMeasure& measure) const;
    bool HandleMouse(const EditorView& view,
                     const TextMeasure& measure);

    void ResetCursorIfInvalid();

    // Called by every edit: the content changed and `source` must be updated.
    // Inside an EditScope this is done once at the end, with an undo step.
    void MarkEdited();
    bool RequestDelete(CursorPath node,
                       int key);
    bool DeleteNodeAt(const CursorPath& node);
    bool ApplyPendingDelete();
    bool HandleWheelKey(int key);

    // Shift+Left/Right: move the moving end of the selection one step, over
    // whole objects at the level where the selection is.
    bool ExtendSelection(bool right);
};
