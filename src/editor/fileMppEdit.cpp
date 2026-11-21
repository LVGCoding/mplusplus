// Editing M++ documents (FileMpp): keys, typing, deleting, inserting.
//
// Keys go down the cursor path as a KeyEvent (see EditorNode.h). Nodes edit
// text themselves; anything that changes the tree above a TextNode (deleting
// an object, splitting rows, matrix rows/columns) comes back up as a KeyResult
// and is done here, so the red "press again to delete" state and the matrix
// insert wheel live in one place.

#include "FileMpp.h"
#include "math/ExpoNode.h"
#include "math/LineNode.h"
#include "math/MatrixNode.h"
#include "math/SubNode.h"
#include "math/TextNode.h"
#include "raylib.h"

#include <algorithm>
#include <iterator>
#include <optional>
#include <utility>

namespace {
CursorPath Prefix(const CursorPath& path,
                  std::size_t dropFromEnd) {
    return CursorPath(path.begin(), path.end() - static_cast<std::ptrdiff_t>(dropFromEnd));
}

CursorPath Concat(CursorPath prefix,
                  const CursorPath& rest) {
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

std::optional<CursorPath> FileMpp::FindNode(std::uint32_t id) const {
    // Depth-first search; documents are small enough that this is instant.
    std::function<std::optional<CursorPath>(const EditorNode&, CursorPath&)> search =
        [&](const EditorNode& node, CursorPath& trail) -> std::optional<CursorPath> {
        if (node.GetId() == id) {
            return trail;
        }
        for (const auto& child : node.ChildPaths()) {
            trail.insert(trail.end(), child.path.begin(), child.path.end());
            if (auto found = search(*child.node, trail)) {
                return found;
            }
            trail.resize(trail.size() - child.path.size());
        }
        return std::nullopt;
    };
    for (std::size_t row = 0; row < content.size(); ++row) {
        CursorPath trail = {row};
        if (auto found = search(*content[row], trail)) {
            return found;
        }
    }
    return std::nullopt;
}

bool FileMpp::EditNode(std::uint32_t id,
                       const std::function<bool(EditorNode&)>& edit) {
    const auto nodePath = FindNode(id);
    if (!nodePath) {
        return false;
    }
    const std::size_t row = (*nodePath)[0];
    EditorNode* node = NodeAt(*nodePath);
    if (!node) {
        return false;
    }
    // The start of the node, or of its row if the node can't hold the cursor.
    auto startOfNode = [&] {
        const CursorPath inside = node->FirstPosition();
        return inside.empty() ? JoinPath({row}, content[row]->FirstPosition())
                              : Concat(*nodePath, inside);
    };
    if (cursorPos.empty() || cursorPos[0] != row || selectionStart[0] != row) {
        MoveCursorTo(startOfNode());
    }
    EditScope scope(*this, EditKind::OTHER, true);
    if (!edit(*node)) {
        return false;
    }
    // The edit may have removed the part holding the cursor (e.g. a row);
    // put it back at the start of the node.
    if (!CursorText(cursorPos) || !CursorText(selectionStart)) {
        cursorPos = startOfNode();
        selectionStart = cursorPos;
    }
    pendingDelete_.reset();
    MarkEdited();
    return true;
}

TextNode* FileMpp::CursorText(const CursorPath& path,
                              std::size_t* offset) const {
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
    if (text.empty()) {
        return false;
    }
    FileMpp pasted;
    pasted.SetSource(std::move(text));
    EditScope scope(*this, EditKind::OTHER);
    DeleteSelection();
    pendingDelete_.reset();
    insertWheel_.reset();
    const CursorPath linePath = Prefix(cursorPos, 2);
    auto* line = dynamic_cast<LineNode*>(NodeAt(linePath));
    if (!line) {
        return false;
    }

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

namespace {
// The spaces a row starts with.
std::string Indentation(const LineNode& row) {
    const auto* text = dynamic_cast<const TextNode*>(row.GetChild(0));
    if (!text) {
        return {};
    }
    const std::string& value = text->GetText();
    return value.substr(0, value.find_first_not_of(' ') == std::string::npos
                               ? value.size()
                               : value.find_first_not_of(' '));
}

constexpr std::size_t INDENT = 4;
}  // namespace

bool FileMpp::PressTab(bool shift) {
    EditScope scope(*this, EditKind::OTHER);
    const auto selection = GetSelection();
    if (!shift && !selection) {
        // Spaces at the cursor up to the next multiple of INDENT, like a tab
        // stop (counted in the row's first text; elsewhere a full INDENT).
        const bool rowText = cursorPos.size() == 3;
        const std::size_t column = rowText && cursorPos[1] == 0 ? cursorPos[2] : 0;
        const std::string spaces(INDENT - column % INDENT, ' ');
        auto* text = rowText ? dynamic_cast<TextNode*>(content[cursorPos[0]]->GetChild(cursorPos[1]))
                             : nullptr;
        if (!text) {
            // Inside a fraction, matrix, ...: indent the row instead.
            ShiftRow(cursorPos[0], static_cast<int>(INDENT));
        } else {
            std::string value = text->GetText();
            value.insert(cursorPos[2], spaces);
            text->SetText(value);
            cursorPos[2] += spaces.size();
            selectionStart = cursorPos;
        }
        MarkEdited();
        return true;
    }
    // Whole rows: the cursor's, or every row the selection touches (not the
    // row where it ends if it ends at that row's very start).
    std::size_t first = cursorPos[0];
    std::size_t last = cursorPos[0];
    if (selection) {
        first = selection->start[0];
        last = selection->end[0];
        const CursorPath& end = selection->end;
        if (last > first && end.size() == 3 && end[1] == 0 && end[2] == 0) {
            --last;
        }
    }
    bool changed = false;
    for (std::size_t row = first; row <= last && row < content.size(); ++row) {
        const std::string before = content[row]->ToText();
        ShiftRow(row, shift ? -static_cast<int>(INDENT) : static_cast<int>(INDENT));
        changed |= content[row]->ToText() != before;
    }
    if (changed) {
        MarkEdited();
    }
    return changed;
}

void FileMpp::ShiftRow(std::size_t row, int spaces) {
    auto* text = dynamic_cast<TextNode*>(content[row]->GetChild(0));
    if (!text) {
        return;
    }
    std::string value = text->GetText();
    std::size_t removed = 0;
    if (spaces >= 0) {
        value.insert(0, static_cast<std::size_t>(spaces), ' ');
    } else {
        const std::size_t leading = std::min(value.find_first_not_of(' '), value.size());
        removed = std::min(leading, static_cast<std::size_t>(-spaces));
        value.erase(0, removed);
    }
    text->SetText(value);
    // Positions in that text move with it.
    for (CursorPath* path : {&cursorPos, &selectionStart}) {
        if (path->size() == 3 && (*path)[0] == row && (*path)[1] == 0) {
            std::size_t& offset = (*path)[2];
            offset = spaces >= 0 ? offset + static_cast<std::size_t>(spaces)
                                 : offset - std::min(offset, removed);
        }
    }
}

// Enter at row level: the new row keeps the indentation, one level more
// after a "{". Between "{" and "}" the "}" goes on a row of its own.
void FileMpp::SplitRow(std::size_t row) {
    const std::string indentation = Indentation(*content[row]);
    bool opens = false;
    if (const auto* text = dynamic_cast<const TextNode*>(content[row]->GetChild(cursorPos[1]))) {
        const std::string before = text->GetText().substr(0, cursorPos[2]);
        const std::size_t last = before.find_last_not_of(' ');
        opens = last != std::string::npos && before[last] == '{';
    }
    auto rest = content[row]->SplitAt(cursorPos[1], cursorPos[2]);
    std::string restText;
    auto* restStart = dynamic_cast<TextNode*>(rest->GetChild(0));
    if (restStart) {
        restText = restStart->GetText();
        restText.erase(0, std::min(restText.find_first_not_of(' '), restText.size()));
    }
    const std::string inner = indentation + (opens ? std::string(INDENT, ' ') : "");
    const bool closes = opens && restText.starts_with('}');
    auto at = content.begin() + static_cast<std::ptrdiff_t>(row) + 1;
    if (closes) {
        restStart->SetText(indentation + restText);
        at = content.insert(at, std::move(rest));
        auto middle = std::make_unique<LineNode>();
        middle->AddChild(std::make_unique<TextNode>(inner));
        middle->Normalize();
        content.insert(at, std::move(middle));
    } else {
        if (restStart) {
            restStart->SetText(inner + restText);
        }
        content.insert(at, std::move(rest));
    }
    cursorPos = {row + 1, 0, inner.size()};
    selectionStart = cursorPos;
    MarkEdited();
}

// True if the cursor is between the quotes of a string in its line.
bool FileMpp::InsideString() const {
    const CursorPath linePath = Prefix(cursorPos, 2);
    const auto* line = dynamic_cast<const LineNode*>(NodeAt(linePath));
    if (!line) {
        return false;
    }
    const std::size_t child = cursorPos[cursorPos.size() - 2];
    bool inside = false;
    for (std::size_t i = 0; i <= child && i < line->ChildCount(); ++i) {
        const auto* text = dynamic_cast<const TextNode*>(line->GetChild(i));
        if (!text) {
            continue;
        }
        const std::string& value = text->GetText();
        const std::size_t end = i == child ? std::min(cursorPos.back(), value.size()) : value.size();
        for (std::size_t k = 0; k < end; ++k) {
            inside ^= value[k] == '"';
        }
    }
    return inside;
}

bool FileMpp::PressKey(int key,
                       bool shift,
                       const TextMeasure* measure) {
    ResetCursorIfInvalid();
    // Only these keys edit (arrows and Escape just move or toggle marks; Tab
    // indents, see PressTab).
    // Deleting single characters groups into one undo step, like typing.
    std::optional<EditScope> scope;
    if (key == KEY_BACKSPACE || key == KEY_DELETE) {
        const bool characters = !pendingDelete_ && !insertWheel_ && !GetSelection();
        scope.emplace(*this, characters ? EditKind::DELETING : EditKind::OTHER);
    } else if (key == KEY_ENTER) {
        scope.emplace(*this, EditKind::OTHER);
    }
    if (insertWheel_) {
        return HandleWheelKey(key);
    }
    if (pendingDelete_) {
        if (key == pendingDelete_->key) {
            return ApplyPendingDelete();
        }
        if (key == KEY_TAB && pendingDelete_->part != DeleteHighlight::Part::WHOLE) {
            pendingDelete_->part = pendingDelete_->part == DeleteHighlight::Part::ROW
                                       ? DeleteHighlight::Part::COLUMN
                                       : DeleteHighlight::Part::ROW;
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
    if (key == KEY_TAB) {
        return PressTab(shift);
    }
    if (key == KEY_ESCAPE) {
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
    case KeyResult::Kind::MOVED:
        next = JoinPath({row}, std::move(result.path));
        if (result.changed) {
            cursorPos = next;
            selectionStart = cursorPos;
            MarkEdited();
            return true;
        }
        break;
    case KeyResult::Kind::EXIT_LEFT:
        if (row > 0) {
            next = JoinPath({row - 1}, content[row - 1]->LastPosition());
        }
        break;
    case KeyResult::Kind::EXIT_RIGHT:
        if (row + 1 < content.size()) {
            next = JoinPath({row + 1}, content[row + 1]->FirstPosition());
        }
        break;
    case KeyResult::Kind::EXIT_UP:
        if (row > 0) {
            next = JoinPath(
                {row - 1},
                content[row - 1]->PositionNearX(event.cursorX, VerticalEntry::FROM_BELOW, measure));
        }
        break;
    case KeyResult::Kind::EXIT_DOWN:
        if (row + 1 < content.size()) {
            next = JoinPath(
                {row + 1},
                content[row + 1]->PositionNearX(event.cursorX, VerticalEntry::FROM_ABOVE, measure));
        }
        break;
    case KeyResult::Kind::DELETE_LEFT:
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
    case KeyResult::Kind::DELETE_RIGHT:
        // Delete at the end of a row pulls the next row up.
        if (row + 1 >= content.size()) {
            return false;
        }
        content[row]->Append(*content[row + 1]);
        content.erase(content.begin() + static_cast<std::ptrdiff_t>(row) + 1);
        MarkEdited();
        return true;
    case KeyResult::Kind::DELETE_NODE:
        return RequestDelete(JoinPath({row}, std::move(result.path)), key);
    case KeyResult::Kind::MATRIX_LINE: {
        const CursorPath cell = JoinPath({row}, std::move(result.path));
        pendingDelete_ = PendingDelete{
            Prefix(cell, 2), DeleteHighlight::Part::ROW, cell[cell.size() - 2], cell.back(), key};
        return true;
    }
    case KeyResult::Kind::MATRIX_INSERT: {
        const CursorPath cell = JoinPath({row}, std::move(result.path));
        insertWheel_ = InsertWheel{Prefix(cell, 2), cell[cell.size() - 2], cell.back(), {}};
        return true;
    }
    case KeyResult::Kind::DELETE_SELF:
        return false;  // rows are never deleted this way
    case KeyResult::Kind::IGNORED:
        // Enter directly on a row (not inside an object) splits the row.
        if (key == KEY_ENTER && cursorPos.size() == 3) {
            SplitRow(row);
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
    if (codepoint == U'_' && !InsideString()) {
        return InsertNode(std::make_unique<SubNode>(std::make_unique<LineNode>()));
    }
    if (codepoint == U'^') {
        return InsertNode(std::make_unique<ExpoNode>(std::make_unique<LineNode>()));
    }
    // A space ends the word, so each word (with its space) undoes in one go.
    EditScope scope(*this, EditKind::TYPING, codepoint == U' ');
    DeleteSelection();  // typing replaces the selection
    // A "}" typed after only spaces closes a block: undo one level of indentation.
    if (codepoint == U'}' && cursorPos.size() == 3 && cursorPos[1] == 0) {
        if (auto* text = dynamic_cast<TextNode*>(content[cursorPos[0]]->GetChild(0))) {
            const std::string& value = text->GetText();
            const std::size_t offset = cursorPos[2];
            if (offset > 0 && value.find_first_not_of(' ') >= offset) {
                const std::size_t remove = std::min(offset, INDENT);
                text->SetText(value.substr(0, offset - remove) + value.substr(offset));
                cursorPos[2] -= remove;
                selectionStart = cursorPos;
            }
        }
    }
    KeyEvent event;
    event.codepoint = codepoint;
    event.path = PathView(cursorPos).subspan(1);
    KeyResult result = content[cursorPos[0]]->KeyPress(event);
    if (result.kind != KeyResult::Kind::MOVED) {
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
    EditScope scope(*this, EditKind::OTHER);
    pendingDelete_.reset();
    insertWheel_.reset();
    DeleteSelection();
    const CursorPath linePath = Prefix(cursorPos, 2);
    auto* line = dynamic_cast<LineNode*>(NodeAt(linePath));
    if (!line || !node) {
        return false;
    }
    const std::size_t index =
        line->InsertAtText(cursorPos[cursorPos.size() - 2], cursorPos.back(), std::move(node));
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

bool FileMpp::RequestDelete(CursorPath node,
                            int key) {
    EditorNode* target = NodeAt(node);
    if (!target) {
        return false;
    }
    // Symbols like \alpha go at once, like a character; objects with slots
    // follow the setting.
    const bool symbol = target->FirstPosition().empty();
    if (symbol || backspaceMode == editor_settings::StructureBackspace::DELETE_IMMEDIATELY) {
        return DeleteNodeAt(node);
    }
    pendingDelete_ = PendingDelete{std::move(node), DeleteHighlight::Part::WHOLE, 0, 0, key};
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
    if (pending.part == DeleteHighlight::Part::WHOLE) {
        return DeleteNodeAt(pending.node);
    }
    auto* matrix = dynamic_cast<MatrixNode*>(NodeAt(pending.node));
    if (!matrix) {
        return true;
    }
    const bool removed = pending.part == DeleteHighlight::Part::ROW
                             ? matrix->RemoveRow(pending.row)
                             : matrix->RemoveColumn(pending.column);
    if (!removed) {
        return true;  // last row/column: just clear the highlight
    }
    // Stay in the same place, or the nearest cell that is left.
    const std::size_t row = std::min(pending.row, matrix->RowCount() - 1);
    const std::size_t columns = matrix->ColumnCount(row);
    const std::size_t column = std::min(pending.column, columns > 0 ? columns - 1 : 0);
    cursorPos = Concat(pending.node,
                       JoinPath({row, column}, matrix->GetCell(row, column)->FirstPosition()));
    selectionStart = cursorPos;
    MarkEdited();
    return true;
}

bool FileMpp::HandleWheelKey(int key) {
    InsertWheel& wheel = *insertWheel_;
    switch (key) {
    case KEY_UP:
        wheel.direction = WheelDirection::UP;
        return true;
    case KEY_DOWN:
        wheel.direction = WheelDirection::DOWN;
        return true;
    case KEY_LEFT:
        wheel.direction = WheelDirection::LEFT;
        return true;
    case KEY_RIGHT:
        wheel.direction = WheelDirection::RIGHT;
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
    case WheelDirection::UP:
        row = matrix->InsertRow(row);
        break;
    case WheelDirection::DOWN:
        row = matrix->InsertRow(row + 1);
        break;
    case WheelDirection::LEFT:
        matrix->InsertColumn(column);
        break;
    case WheelDirection::RIGHT:
        matrix->InsertColumn(++column);
        break;
    }
    // Move into the new cell.
    column = std::min(column, matrix->ColumnCount(row) - 1);
    cursorPos = Concat(chosen.matrix,
                       JoinPath({row, column}, matrix->GetCell(row, column)->FirstPosition()));
    selectionStart = cursorPos;
    MarkEdited();
    return true;
}
