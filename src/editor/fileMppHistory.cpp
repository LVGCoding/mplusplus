// Undo and redo for M++ documents (FileMpp).
//
// Every user action that can edit (a key, a typed character, inserting an
// object) runs inside an EditScope. Before it starts, the rows it could touch
// are copied: from one row above the selection (or cursor) to one row below,
// because Backspace/Delete at a row's edge join it with its neighbour. When
// it ends, rows that didn't change are trimmed off both ends and the copies of
// the rest become the undo step. Steps hold node copies, not text, so undo
// puts back exactly the tree that was there and the saved cursor paths stay
// valid.

#include "FileMpp.h"

#include <algorithm>
#include <utility>

namespace {
std::size_t RowOf(const CursorPath& path) {
    return path.empty() ? 0 : path[0];
}

std::unique_ptr<LineNode> CloneRow(const LineNode& row) {
    auto copy = row.Clone();
    return std::unique_ptr<LineNode>(static_cast<LineNode*>(copy.release()));
}
}  // namespace

void FileMpp::MarkEdited() {
    goalX_.reset();
    if (editDepth_ > 0) {
        edited_ = true;  // EndEdit() records the step and updates `source`
        return;
    }
    UpdateSource();
}

void FileMpp::UpdateSource() {
    source = ContentText();
    modified = !savedSource_ || source != *savedSource_;
}

void FileMpp::ClearHistory() {
    undo_.clear();
    redo_.clear();
    openGroup_.reset();
}

void FileMpp::BeginEdit(EditKind kind, bool endsGroup) {
    if (editDepth_++ > 0) {
        return;  // part of the action already in progress
    }
    ResetCursorIfInvalid();
    edited_ = false;
    const std::size_t low = std::min(RowOf(cursorPos), RowOf(selectionStart));
    const std::size_t high = std::max(RowOf(cursorPos), RowOf(selectionStart));
    OpenEdit edit;
    edit.firstRow = low > 0 ? low - 1 : 0;
    const std::size_t lastRow = std::min(high + 1, content.size() - 1);
    edit.rowCount = content.size();
    for (std::size_t row = edit.firstRow; row <= lastRow; ++row) {
        edit.rows.push_back(CloneRow(*content[row]));
        edit.texts.push_back(content[row]->ToText());
    }
    edit.cursor = cursorPos;
    edit.anchor = selectionStart;
    edit.kind = kind;
    edit.endsGroup = endsGroup;
    openEdit_ = std::move(edit);
}

void FileMpp::EndEdit() {
    if (--editDepth_ > 0) {
        return;
    }
    OpenEdit edit = std::move(*openEdit_);
    openEdit_.reset();
    if (!std::exchange(edited_, false)) {
        return;
    }

    // The copied window, before and after; the edit changed nothing outside it.
    const std::size_t windowRows = edit.rows.size();
    const std::size_t newWindowRows = windowRows + content.size() - edit.rowCount;
    std::size_t first = 0;
    std::size_t oldEnd = windowRows;
    std::size_t newEnd = newWindowRows;
    while (first < oldEnd && first < newEnd &&
           content[edit.firstRow + first]->ToText() == edit.texts[first]) {
        ++first;
    }
    while (oldEnd > first && newEnd > first &&
           content[edit.firstRow + newEnd - 1]->ToText() == edit.texts[oldEnd - 1]) {
        --oldEnd;
        --newEnd;
    }
    UpdateSource();
    if (first == oldEnd && first == newEnd) {
        return;  // edited back to what it was
    }

    UndoStep step;
    step.firstRow = edit.firstRow + first;
    step.rows.assign(std::make_move_iterator(edit.rows.begin() + static_cast<std::ptrdiff_t>(first)),
                     std::make_move_iterator(edit.rows.begin() + static_cast<std::ptrdiff_t>(oldEnd)));
    step.replaces = newEnd - first;
    step.cursorBefore = std::move(edit.cursor);
    step.anchorBefore = std::move(edit.anchor);
    step.cursorAfter = cursorPos;
    step.anchorAfter = selectionStart;
    redo_.clear();

    // Typing or deleting characters straight after the last step, in the same
    // single row, joins it.
    if (!undo_.empty() && openGroup_ == edit.kind && edit.kind != EditKind::Other) {
        UndoStep& last = undo_.back();
        if (last.firstRow == step.firstRow && last.replaces == 1 && step.rows.size() == 1 &&
            step.replaces == 1 && last.cursorAfter == step.cursorBefore &&
            step.anchorBefore == step.cursorBefore) {
            last.cursorAfter = std::move(step.cursorAfter);
            last.anchorAfter = std::move(step.anchorAfter);
            openGroup_ = edit.endsGroup ? std::nullopt : std::optional(edit.kind);
            return;
        }
    }
    undo_.push_back(std::move(step));
    if (undo_.size() > maxUndoSteps) {
        undo_.pop_front();
    }
    openGroup_ = edit.endsGroup ? std::nullopt : std::optional(edit.kind);
}

void FileMpp::SwapRows(UndoStep& step) {
    const auto at = content.begin() + static_cast<std::ptrdiff_t>(step.firstRow);
    std::vector<std::unique_ptr<LineNode>> current(
        std::make_move_iterator(at), std::make_move_iterator(at + static_cast<std::ptrdiff_t>(step.replaces)));
    content.erase(at, at + static_cast<std::ptrdiff_t>(step.replaces));
    content.insert(content.begin() + static_cast<std::ptrdiff_t>(step.firstRow),
                   std::make_move_iterator(step.rows.begin()),
                   std::make_move_iterator(step.rows.end()));
    step.replaces = step.rows.size();
    step.rows = std::move(current);
}

void FileMpp::AfterHistoryMove() {
    openGroup_.reset();
    pendingDelete_.reset();
    insertWheel_.reset();
    goalX_.reset();
    ResetCursorIfInvalid();
    UpdateSource();
}

bool FileMpp::Undo() {
    if (undo_.empty() || editDepth_ > 0) {
        return false;
    }
    UndoStep step = std::move(undo_.back());
    undo_.pop_back();
    SwapRows(step);
    cursorPos = step.cursorBefore;
    selectionStart = step.anchorBefore;
    redo_.push_back(std::move(step));
    AfterHistoryMove();
    return true;
}

bool FileMpp::Redo() {
    if (redo_.empty() || editDepth_ > 0) {
        return false;
    }
    UndoStep step = std::move(redo_.back());
    redo_.pop_back();
    SwapRows(step);
    cursorPos = step.cursorAfter;
    selectionStart = step.anchorAfter;
    undo_.push_back(std::move(step));
    AfterHistoryMove();
    return true;
}
