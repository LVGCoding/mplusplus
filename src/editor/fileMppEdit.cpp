// Editing M++ documents (FileMpp): keys, typing, deleting, inserting.
//
// Keys go down the cursor path as a KeyEvent (see EditorNode.h). Nodes edit
// text themselves; anything that changes the tree above a TextNode (deleting
// an object, splitting rows, matrix rows/columns) comes back up as a KeyResult
// and is done here, so the red "press again to delete" state and the matrix
// insert wheel live in one place.

#include "FileMpp.h"

#include "math/LineNode.h"
#include "math/MatrixNode.h"
#include "math/TextNode.h"
#include "raylib.h"

#include <algorithm>
#include <iterator>
#include <optional>
#include <utility>

namespace {
CursorPath Prefix(const CursorPath& path, std::size_t dropFromEnd) {
    return CursorPath(path.begin(), path.end() - static_cast<std::ptrdiff_t>(dropFromEnd));
}

CursorPath Concat(CursorPath prefix, const CursorPath& rest) {
    prefix.insert(prefix.end(), rest.begin(), rest.end());
    return prefix;
}
}  // namespace

EditorNode* FileMpp::NodeAt(PathView path) const {
    if (path.empty() || path[0] >= content.size()) {
        return nullptr;
    }
    EditorNode* node = content[path[0]].get();
    PathView rest = path.subspan(1);
    while (!rest.empty()) {
        std::size_t used = 0;
        node = node->PathChild(rest, used);
        if (!node) {
            return nullptr;
        }
        rest = rest.subspan(used);
    }
    return node;
}

TextNode* FileMpp::CursorText(const CursorPath& path, std::size_t* offset) const {
    if (path.size() < 2) {
        return nullptr;
    }
    auto* text = dynamic_cast<TextNode*>(NodeAt(PathView(path).first(path.size() - 1)));
    if (!text || path.back() > text->GetText().size()) {
        return nullptr;
    }
    if (offset) {
        *offset = path.back();
    }
    return text;
}

void FileMpp::ResetCursorIfInvalid() {
    if (!CursorText(cursorPos)) {
        cursorPos = JoinPath({0}, content.front()->FirstPosition());
        selectionStart = cursorPos;
    }
    if (!CursorText(selectionStart)) {
        selectionStart = cursorPos;
    }
}

bool FileMpp::TakeMathMenuRequest() {
    return std::exchange(mathMenuRequested_, false);
}

bool FileMpp::PasteText(std::string text) {
    if (text.empty()) return false;
    FileMpp pasted;
    pasted.SetSource(std::move(text));
    EditScope scope(*this, EditKind::Other);
    DeleteSelection();
    pendingDelete_.reset();
    insertWheel_.reset();
    const CursorPath linePath = Prefix(cursorPos, 2);
    auto* line = dynamic_cast<LineNode*>(NodeAt(linePath));
    if (!line) return false;

    // Slots are single lines; retain pasted math objects but treat document
    // line breaks as whitespace when pasting inside a fraction/root/cell.
    if (linePath.size() > 1 && pasted.content.size() > 1) {
        for (std::size_t i = 1; i < pasted.content.size(); ++i) {
            pasted.content.front()->AddChild(std::make_unique<TextNode>(" "));
            pasted.content.front()->Append(*pasted.content[i]);
        }
        pasted.content.resize(1);
    }
    auto tail = line->SplitAt(cursorPos[cursorPos.size() - 2], cursorPos.back());
    line->Append(*pasted.content.front());
    LineNode* last = line;
    CursorPath destination = linePath;
    if (pasted.content.size() > 1) {
        const auto row = cursorPos[0];
        destination = {row + pasted.content.size() - 1};
        last = pasted.content.back().get();
        content.insert(content.begin() + static_cast<std::ptrdiff_t>(row) + 1,
                       std::make_move_iterator(pasted.content.begin() + 1),
                       std::make_move_iterator(pasted.content.end()));
    }
    cursorPos = Concat(destination, last->LastPosition());
    last->Append(*tail);
    selectionStart = cursorPos;
    MarkEdited();
    return true;
}

bool FileMpp::PressKey(int key, bool shift, const TextMeasure* measure) {
    ResetCursorIfInvalid();
    // Only these keys edit (arrows, Tab and Escape just move or toggle marks).
    // Deleting single characters groups into one undo step, like typing.
    std::optional<EditScope> scope;
    if (key == KEY_BACKSPACE || key == KEY_DELETE) {
        const bool characters = !pendingDelete_ && !insertWheel_ && !GetSelection();
        scope.emplace(*this, characters ? EditKind::Deleting : EditKind::Other);
    } else if (key == KEY_ENTER) {
        scope.emplace(*this, EditKind::Other);
    }
    if (insertWheel_) {
        return HandleWheelKey(key);
    }
    if (pendingDelete_) {
        if (key == pendingDelete_->key) {
            return ApplyPendingDelete();
        }
        if (key == KEY_TAB && pendingDelete_->part != DeleteHighlight::Part::Whole) {
            pendingDelete_->part = pendingDelete_->part == DeleteHighlight::Part::Row
                                       ? DeleteHighlight::Part::Column
                                       : DeleteHighlight::Part::Row;
            return true;
        }
        // Any other key cancels; Escape does nothing else.
        pendingDelete_.reset();
        if (key == KEY_ESCAPE) {
            return true;
        }
    }

    // With something selected: Escape clears it, Left/Right go to its ends,
    // Backspace/Delete remove it and Enter replaces it.
    if (GetSelection() && !shift) {
        switch (key) {
        case KEY_ESCAPE:
            selectionStart = cursorPos;
            return true;
        case KEY_LEFT:
        case KEY_RIGHT:
            cursorPos = key == KEY_LEFT ? GetSelection()->start : GetSelection()->end;
            selectionStart = cursorPos;
            goalX_.reset();
            return true;
        default:
            break;
        }
    }
    if (key == KEY_BACKSPACE || key == KEY_DELETE) {
        if (DeleteSelection()) {
            return true;
        }
    }
    if (key == KEY_ENTER) {
        DeleteSelection();
    }
    if (shift && (key == KEY_LEFT || key == KEY_RIGHT)) {
        return ExtendSelection(key == KEY_RIGHT);
    }
    if (key == KEY_ESCAPE || key == KEY_TAB) {
        return false;
    }

    const bool vertical = key == KEY_UP || key == KEY_DOWN;
    if (!vertical || cursorPos != goalPath_) {
        goalX_.reset();
    }
    if (vertical && !goalX_) {
        const auto caret = CaretBox(measure);
        goalX_ = caret ? caret->x : 0.0f;
    }

    const std::size_t row = cursorPos[0];
    KeyEvent event;
    event.key = key;
    event.shift = shift;
    event.path = PathView(cursorPos).subspan(1);
    event.cursorX = goalX_.value_or(0.0f);
    event.measure = measure;
    KeyResult result = content[row]->KeyPress(event);

    CursorPath next;
    switch (result.kind) {
    case KeyResult::Kind::Moved:
        next = JoinPath({row}, std::move(result.path));
        if (result.changed) {
            cursorPos = next;
            selectionStart = cursorPos;
            MarkEdited();
            return true;
        }
        break;
    case KeyResult::Kind::ExitLeft:
        if (row > 0) {
            next = JoinPath({row - 1}, content[row - 1]->LastPosition());
        }
        break;
    case KeyResult::Kind::ExitRight:
        if (row + 1 < content.size()) {
            next = JoinPath({row + 1}, content[row + 1]->FirstPosition());
        }
        break;
    case KeyResult::Kind::ExitUp:
        if (row > 0) {
            next = JoinPath({row - 1}, content[row - 1]->PositionNearX(
                                           event.cursorX, VerticalEntry::FromBelow, measure));
        }
        break;
    case KeyResult::Kind::ExitDown:
        if (row + 1 < content.size()) {
            next = JoinPath({row + 1}, content[row + 1]->PositionNearX(
                                           event.cursorX, VerticalEntry::FromAbove, measure));
        }
        break;
    case KeyResult::Kind::DeleteLeft:
        // Backspace at the start of a row joins it onto the row above.
        if (row == 0) {
            return false;
        }
        cursorPos = JoinPath({row - 1}, content[row - 1]->LastPosition());
        content[row - 1]->Append(*content[row]);
        content.erase(content.begin() + static_cast<std::ptrdiff_t>(row));
        selectionStart = cursorPos;
        MarkEdited();
        return true;
    case KeyResult::Kind::DeleteRight:
        // Delete at the end of a row pulls the next row up.
        if (row + 1 >= content.size()) {
            return false;
        }
        content[row]->Append(*content[row + 1]);
        content.erase(content.begin() + static_cast<std::ptrdiff_t>(row) + 1);
        MarkEdited();
        return true;
    case KeyResult::Kind::DeleteNode:
        return RequestDelete(JoinPath({row}, std::move(result.path)), key);
    case KeyResult::Kind::MatrixLine: {
        const CursorPath cell = JoinPath({row}, std::move(result.path));
        pendingDelete_ = PendingDelete{Prefix(cell, 2), DeleteHighlight::Part::Row,
                                       cell[cell.size() - 2], cell.back(), key};
        return true;
    }
    case KeyResult::Kind::MatrixInsert: {
        const CursorPath cell = JoinPath({row}, std::move(result.path));
        insertWheel_ = InsertWheel{Prefix(cell, 2), cell[cell.size() - 2], cell.back(), {}};
        return true;
    }
    case KeyResult::Kind::DeleteSelf:
        return false;  // rows are never deleted this way
    case KeyResult::Kind::Ignored:
        // Enter directly on a row (not inside an object) splits the row.
        if (key == KEY_ENTER && cursorPos.size() == 3) {
            auto rest = content[row]->SplitAt(cursorPos[1], cursorPos[2]);
            content.insert(content.begin() + static_cast<std::ptrdiff_t>(row) + 1, std::move(rest));
            cursorPos = JoinPath({row + 1}, content[row + 1]->FirstPosition());
            selectionStart = cursorPos;
            MarkEdited();
            return true;
        }
        return false;
    }

    if (next.empty() || next == cursorPos) {
        // Up/Down that can't move still drops the selection.
        if (!shift && selectionStart != cursorPos) {
            selectionStart = cursorPos;
            return true;
        }
        return false;
    }
    cursorPos = std::move(next);
    if (vertical) {
        goalPath_ = cursorPos;
    }
    if (!shift) {
        selectionStart = cursorPos;
    }
    return true;
}

bool FileMpp::TypeCharacter(char32_t codepoint) {
    if (insertWheel_ || codepoint < 32 || codepoint == 127) {
        return false;
    }
    ResetCursorIfInvalid();
    pendingDelete_.reset();
    if (codepoint == U'\\') {
        mathMenuRequested_ = true;
        return true;
    }
    // Braces would unbalance the LaTeX when saved; objects are inserted from
    // the math popup instead.
    if (codepoint == U'{' || codepoint == U'}') {
        return false;
    }
    // A space ends the word, so each word (with its space) undoes in one go.
    EditScope scope(*this, EditKind::Typing, codepoint == U' ');
    DeleteSelection();  // typing replaces the selection
    KeyEvent event;
    event.codepoint = codepoint;
    event.path = PathView(cursorPos).subspan(1);
    KeyResult result = content[cursorPos[0]]->KeyPress(event);
    if (result.kind != KeyResult::Kind::Moved) {
        return false;
    }
    cursorPos = JoinPath({cursorPos[0]}, std::move(result.path));
    selectionStart = cursorPos;
    if (result.changed) {
        MarkEdited();
    }
    return true;
}

bool FileMpp::InsertNode(std::unique_ptr<EditorNode> node) {
    ResetCursorIfInvalid();
    EditScope scope(*this, EditKind::Other);
    pendingDelete_.reset();
    insertWheel_.reset();
    DeleteSelection();
    const CursorPath linePath = Prefix(cursorPos, 2);
    auto* line = dynamic_cast<LineNode*>(NodeAt(linePath));
    if (!line || !node) {
        return false;
    }
    const std::size_t index = line->InsertAtText(cursorPos[cursorPos.size() - 2], cursorPos.back(),
                                                 std::move(node));
    // Move into the new object; one the cursor can't enter is stepped over.
    CursorPath inside = JoinPath({index}, line->GetChild(index)->FirstPosition());
    if (inside.empty()) {
        inside = {index + 1, 0};
    }
    cursorPos = Concat(linePath, inside);
    selectionStart = cursorPos;
    MarkEdited();
    return true;
}

bool FileMpp::RequestDelete(CursorPath node, int key) {
    EditorNode* target = NodeAt(node);
    if (!target) {
        return false;
    }
    // Symbols like \alpha go at once, like a character; objects with slots
    // follow the setting.
    const bool symbol = target->FirstPosition().empty();
    if (symbol || backspaceMode == editor_settings::StructureBackspace::DeleteImmediately) {
        return DeleteNodeAt(node);
    }
    pendingDelete_ = PendingDelete{std::move(node), DeleteHighlight::Part::Whole, 0, 0, key};
    return true;
}

bool FileMpp::DeleteNodeAt(const CursorPath& node) {
    if (node.size() < 2) {
        return false;
    }
    const CursorPath linePath = Prefix(node, 1);
    auto* line = dynamic_cast<LineNode*>(NodeAt(linePath));
    if (!line) {
        return false;
    }
    cursorPos = Concat(linePath, line->RemoveAndJoin(node.back()));
    selectionStart = cursorPos;
    pendingDelete_.reset();
    MarkEdited();
    return true;
}

bool FileMpp::ApplyPendingDelete() {
    const PendingDelete pending = *pendingDelete_;
    pendingDelete_.reset();
    if (pending.part == DeleteHighlight::Part::Whole) {
        return DeleteNodeAt(pending.node);
    }
    auto* matrix = dynamic_cast<MatrixNode*>(NodeAt(pending.node));
    if (!matrix) {
        return true;
    }
    const bool removed = pending.part == DeleteHighlight::Part::Row
                             ? matrix->RemoveRow(pending.row)
                             : matrix->RemoveColumn(pending.column);
    if (!removed) {
        return true;  // last row/column: just clear the highlight
    }
    // Stay in the same place, or the nearest cell that is left.
    const std::size_t row = std::min(pending.row, matrix->RowCount() - 1);
    const std::size_t columns = matrix->ColumnCount(row);
    const std::size_t column = std::min(pending.column, columns > 0 ? columns - 1 : 0);
    cursorPos = Concat(pending.node, JoinPath({row, column}, matrix->GetCell(row, column)->FirstPosition()));
    selectionStart = cursorPos;
    MarkEdited();
    return true;
}

bool FileMpp::HandleWheelKey(int key) {
    InsertWheel& wheel = *insertWheel_;
    switch (key) {
    case KEY_UP:
        wheel.direction = WheelDirection::Up;
        return true;
    case KEY_DOWN:
        wheel.direction = WheelDirection::Down;
        return true;
    case KEY_LEFT:
        wheel.direction = WheelDirection::Left;
        return true;
    case KEY_RIGHT:
        wheel.direction = WheelDirection::Right;
        return true;
    case KEY_ESCAPE:
        insertWheel_.reset();
        return true;
    case KEY_ENTER:
        break;
    default:
        return false;
    }

    const InsertWheel chosen = wheel;
    insertWheel_.reset();
    auto* matrix = dynamic_cast<MatrixNode*>(NodeAt(chosen.matrix));
    if (!matrix || !chosen.direction) {
        return true;  // Enter with nothing selected just closes the wheel
    }
    std::size_t row = chosen.row;
    std::size_t column = chosen.column;
    switch (*chosen.direction) {
    case WheelDirection::Up:
        matrix->InsertRow(row);
        break;
    case WheelDirection::Down:
        matrix->InsertRow(++row);
        break;
    case WheelDirection::Left:
        matrix->InsertColumn(column);
        break;
    case WheelDirection::Right:
        matrix->InsertColumn(++column);
        break;
    }
    // Move into the new cell.
    column = std::min(column, matrix->ColumnCount(row) - 1);
    cursorPos = Concat(chosen.matrix, JoinPath({row, column}, matrix->GetCell(row, column)->FirstPosition()));
    selectionStart = cursorPos;
    MarkEdited();
    return true;
}
