#pragma once

#include "EditorSettings.h"
#include "fileBase.h"
#include "math/EditorNode.h"
#include "math/LineNode.h"

#include <cstddef>
#include <deque>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class TextNode;

// An M++ (.mpp) document currently open in the M++ editor.
struct FileMpp : FileBase {
    // Raw LaTeX/M++ source as read from disk. Kept so the file round-trips on
    // save until the node tree can be serialized back to text.
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
    editor_settings::StructureBackspace backspaceMode = editor_settings::structureBackspace;

    // Something marked in red, deleted by pressing the same key again.
    struct PendingDelete {
        CursorPath node;  // the node (Whole) or the matrix (Row/Column)
        DeleteHighlight::Part part = DeleteHighlight::Part::Whole;
        std::size_t row = 0;
        std::size_t column = 0;
        int key = 0;  // KEY_BACKSPACE or KEY_DELETE
    };

    // The wheel around a matrix cell for inserting a row or column.
    enum class WheelDirection { Up, Down, Left, Right };
    struct InsertWheel {
        CursorPath matrix;
        std::size_t row = 0;
        std::size_t column = 0;
        std::optional<WheelDirection> direction;
    };

    FileMpp();

    bool Load(const std::filesystem::path& path, std::string& error) override;
    bool Save(const std::filesystem::path& path, std::string& error) override;

    // Replace the document with parsed `text` and put the cursor at the start.
    void SetSource(std::string text);

    // The document serialized back to M++ source.
    std::string ContentText() const;

    // --- Editing (fileMppEdit.cpp) -------------------------------------------------

    // Handle a key (raylib KEY_*): arrows, Backspace, Delete, Enter, Tab, Escape.
    // `measure` may be null (e.g. in tests); then Up/Down land at the start of
    // the target. Returns true if the cursor, content or highlights changed.
    bool PressKey(int key, bool shift, const TextMeasure* measure);

    // Type a character at the cursor. A backslash asks for the math popup
    // instead (see TakeMathMenuRequest). Returns true if anything changed.
    bool TypeCharacter(char32_t codepoint);

    // Insert a new node (e.g. an empty fraction) at the cursor and move into it.
    bool InsertNode(std::unique_ptr<EditorNode> node);

    // True once after a backslash was typed: the UI should open the math popup.
    bool TakeMathMenuRequest();

    // Place the cursor using screen coordinates from the latest layout.
    // Returns false if no rendered position is available.
    bool ClickAt(float x, float y, const TextMeasure* measure);

    // --- Selection (fileMppSelection.cpp) ----------------------------------------

    // Put the cursor at `path` with nothing selected.
    void MoveCursorTo(CursorPath path);

    // The selected range, or nothing when the selection is empty.
    std::optional<Selection> GetSelection() const;

    // Select the whole document.
    void SelectAll();

    // Delete the selected range. Returns false if nothing was selected.
    bool DeleteSelection();

    // The cursor position nearest to a screen point, from the last layout.
    // Used by ClickAt and by drag-selecting. In fileMppView.cpp.
    std::optional<CursorPath> PositionAt(float x, float y, const TextMeasure* measure) const;

    // --- Undo and redo (fileMppHistory.cpp) ---------------------------------------
    //
    // Each step holds copies of only the rows an edit changed, so history stays
    // small however large the document is. Typing (and Backspace/Delete on
    // characters) in one place is grouped into one step per word.

    bool Undo();
    bool Redo();
    bool CanUndo() const { return !undo_.empty(); }
    bool CanRedo() const { return !redo_.empty(); }

    const std::optional<PendingDelete>& GetPendingDelete() const { return pendingDelete_; }
    const std::optional<InsertWheel>& GetInsertWheel() const { return insertWheel_; }

    // --- Tree lookup -------------------------------------------------------------------

    // The node a path (without a trailing text offset) leads to, or null.
    EditorNode* NodeAt(PathView path) const;

    // The node that `path` ends in and the offset into it, or null if the path
    // doesn't lead to a TextNode.
    TextNode* CursorText(const CursorPath& path, std::size_t* offset = nullptr) const;

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
    std::optional<InsertWheel> insertWheel_;
    bool mathMenuRequested_ = false;

    // True while a mouse drag that started in this file is selecting.
    bool dragging_ = false;

    // --- Undo history (fileMppHistory.cpp) ---

    // Edits that can be grouped with the previous step of the same kind.
    enum class EditKind { Other, Typing, Deleting };

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
        EditKind kind = EditKind::Other;
        bool endsGroup = false;
    };

    // Brackets one user action (a key, a typed character, an insert) so all
    // the changes it makes become a single undo step. Nests.
    class EditScope {
    public:
        EditScope(FileMpp& file, EditKind kind, bool endsGroup = false) : file_(file) {
            file_.BeginEdit(kind, endsGroup);
        }
        ~EditScope() { file_.EndEdit(); }
        EditScope(const EditScope&) = delete;
        EditScope& operator=(const EditScope&) = delete;

    private:
        FileMpp& file_;
    };

    static constexpr std::size_t maxUndoSteps = 1000;

    std::deque<UndoStep> undo_;
    std::deque<UndoStep> redo_;
    std::optional<OpenEdit> openEdit_;
    int editDepth_ = 0;
    bool edited_ = false;
    // Kind of undo_.back() while the next edit may still join it.
    std::optional<EditKind> openGroup_;
    // The text last loaded or saved; nullopt for a file never saved.
    std::optional<std::string> savedSource_;

    void BeginEdit(EditKind kind, bool endsGroup);
    void EndEdit();
    void UpdateSource();
    void ClearHistory();
    // Swap a step's rows with the rows they replace.
    void SwapRows(UndoStep& step);
    void AfterHistoryMove();

    std::optional<Clay_BoundingBox> CaretBox(const TextMeasure* measure) const;
    void DrawInsertWheel(const EditorView& view) const;
    void DrawSelection(const EditorView& view) const;
    bool HandleMouse(const EditorView& view, const TextMeasure& measure);

    void ResetCursorIfInvalid();

    // Called by every edit: the content changed and `source` must be updated.
    // Inside an EditScope this is done once at the end, with an undo step.
    void MarkEdited();
    bool RequestDelete(CursorPath node, int key);
    bool DeleteNodeAt(const CursorPath& node);
    bool ApplyPendingDelete();
    bool HandleWheelKey(int key);

    // Shift+Left/Right: move the moving end of the selection one step, over
    // whole objects at the level where the selection is.
    bool ExtendSelection(bool right);
};
