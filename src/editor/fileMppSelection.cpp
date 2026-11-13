// Selecting in M++ documents (FileMpp).
//
// The selection runs from selectionStart (the anchor) to cursorPos (the end
// that moves). Both are ordinary cursor paths, so they can be anywhere in the
// tree. GetSelection() widens them to the deepest LineNode holding both: if
// one end is inside a fraction and the other isn't, the whole fraction is
// selected, so a selection is always a run of whole nodes plus partial text at
// either end and can be deleted or replaced without breaking the tree.
// Selections that span rows are a range of rows at the top level.

#include "FileMpp.h"

#include "math/LineNode.h"
#include "math/TextNode.h"

#include <algorithm>

namespace {
enum class Side { Before, After };

CursorPath Concat(CursorPath prefix, std::initializer_list<std::size_t> rest) {
    prefix.insert(prefix.end(), rest);
    return prefix;
}

CursorPath Concat(CursorPath prefix, const CursorPath& rest) {
    prefix.insert(prefix.end(), rest.begin(), rest.end());
    return prefix;
}

CursorPath Prefix(const CursorPath& path, std::size_t length) {
    return CursorPath(path.begin(), path.begin() + static_cast<std::ptrdiff_t>(length));
}

std::size_t TextLength(const LineNode& line, std::size_t index) {
    const auto* text = dynamic_cast<const TextNode*>(line.GetChild(index));
    return text ? text->GetText().size() : 0;
}

// Prefix lengths of `path` that lead to a LineNode: 1 for the row itself, then
// one for each fraction slot, root argument or matrix cell it goes into.
std::vector<std::size_t> LineDepths(const FileMpp& file, const CursorPath& path) {
    std::vector<std::size_t> depths;
    if (path.empty() || path[0] >= file.content.size()) {
        return depths;
    }
    depths.push_back(1);
    const EditorNode* node = file.content[path[0]].get();
    std::size_t consumed = 1;
    while (consumed < path.size()) {
        std::size_t used = 0;
        const EditorNode* child = node->PathChild(PathView(path).subspan(consumed), used);
        if (!child) {
            break;
        }
        consumed += used;
        if (dynamic_cast<const LineNode*>(child)) {
            depths.push_back(consumed);
        }
        node = child;
    }
    return depths;
}

// Prefix length of the deepest LineNode holding both paths, or 0 if they are
// in different rows.
std::size_t CommonLineDepth(const FileMpp& file, const CursorPath& a, const CursorPath& b) {
    std::size_t common = 0;
    for (const std::size_t depth : LineDepths(file, a)) {
        if (depth > b.size() || !std::equal(a.begin(), a.begin() + static_cast<std::ptrdiff_t>(depth),
                                            b.begin())) {
            break;
        }
        common = depth;
    }
    return common;
}

// `path` lifted to a position directly in the LineNode at prefix length
// `depth`: unchanged if it already is, otherwise just before or after the
// child of that line it is inside.
CursorPath ClampToLine(const FileMpp& file, const CursorPath& path, std::size_t depth, Side side) {
    if (path.size() <= depth + 2) {
        return path;
    }
    const CursorPath linePath = Prefix(path, depth);
    const auto* line = dynamic_cast<const LineNode*>(file.NodeAt(linePath));
    const std::size_t child = path[depth];
    if (!line || child == 0 || child + 1 >= line->ChildCount()) {
        return path;  // breaks the Normalize() invariant; leave it alone
    }
    return side == Side::Before ? Concat(linePath, {child - 1, TextLength(*line, child - 1)})
                                : Concat(linePath, {child + 1, 0});
}
}  // namespace

std::optional<FileMpp::Selection> FileMpp::GetSelection() const {
    if (selectionStart == cursorPos || !CursorText(selectionStart) || !CursorText(cursorPos)) {
        return std::nullopt;
    }
    // Paths compare in document order.
    const bool forward = selectionStart < cursorPos;
    const CursorPath& first = forward ? selectionStart : cursorPos;
    const CursorPath& last = forward ? cursorPos : selectionStart;
    // Ends in different rows are each lifted to their own row.
    const std::size_t depth = std::max<std::size_t>(CommonLineDepth(*this, first, last), 1);
    Selection selection{ClampToLine(*this, first, depth, Side::Before),
                        ClampToLine(*this, last, depth, Side::After)};
    if (selection.start == selection.end) {
        return std::nullopt;
    }
    return selection;
}

void FileMpp::MoveCursorTo(CursorPath path) {
    cursorPos = std::move(path);
    selectionStart = cursorPos;
    goalX_.reset();
}

void FileMpp::SelectAll() {
    pendingDelete_.reset();
    insertWheel_.reset();
    goalX_.reset();
    selectionStart = JoinPath({0}, content.front()->FirstPosition());
    cursorPos = JoinPath({content.size() - 1}, content.back()->LastPosition());
}

bool FileMpp::DeleteSelection() {
    const auto selection = GetSelection();
    if (!selection) {
        return false;
    }
    EditScope scope(*this, EditKind::Other);
    const CursorPath& start = selection->start;
    const CursorPath& end = selection->end;
    if (start[0] == end[0]) {
        const CursorPath linePath = Prefix(start, start.size() - 2);
        auto* line = dynamic_cast<LineNode*>(NodeAt(linePath));
        if (!line) {
            return false;
        }
        cursorPos = Concat(linePath, line->EraseRange(start[start.size() - 2], start.back(),
                                                      end[end.size() - 2], end.back()));
    } else {
        // Keep the start of the first row and the end of the last, and join them.
        LineNode& top = *content[start[0]];
        LineNode& bottom = *content[end[0]];
        const std::size_t topLast = top.ChildCount() - 1;
        top.EraseRange(start[1], start[2], topLast, TextLength(top, topLast));
        bottom.EraseRange(0, 0, end[1], end[2]);
        top.Append(bottom);
        content.erase(content.begin() + static_cast<std::ptrdiff_t>(start[0]) + 1,
                      content.begin() + static_cast<std::ptrdiff_t>(end[0]) + 1);
        cursorPos = start;
    }
    selectionStart = cursorPos;
    pendingDelete_.reset();
    insertWheel_.reset();
    MarkEdited();
    return true;
}

// The moving end steps through text one character at a time and over objects
// in one step, at the level of the line holding the selection. Leaving a slot
// selects the whole object; stepping back towards an anchor that is inside an
// object goes into the slot holding it.
bool FileMpp::ExtendSelection(bool right) {
    const CursorPath anchor = selectionStart;
    const bool forward = anchor < cursorPos;
    const std::size_t depth = std::max<std::size_t>(CommonLineDepth(*this, anchor, cursorPos), 1);
    const CursorPath focus = ClampToLine(*this, cursorPos, depth, forward ? Side::After : Side::Before);

    const CursorPath linePath = Prefix(focus, focus.size() - 2);
    const auto* line = dynamic_cast<const LineNode*>(NodeAt(linePath));
    std::size_t offset = 0;
    const TextNode* text = CursorText(focus, &offset);
    if (!line || !text) {
        return false;
    }
    const std::size_t child = focus[focus.size() - 2];
    const std::size_t row = focus[0];
    const std::vector<std::size_t> depths = LineDepths(*this, focus);

    // The slot of child `index` that holds the anchor, if it is in there.
    auto anchorSlot = [&](std::size_t index) -> CursorPath {
        if (anchor.size() <= linePath.size() + 2 ||
            !std::equal(linePath.begin(), linePath.end(), anchor.begin()) ||
            anchor[linePath.size()] != index) {
            return {};
        }
        for (const std::size_t slotDepth : LineDepths(*this, anchor)) {
            if (slotDepth > linePath.size()) {
                return Prefix(anchor, slotDepth);
            }
        }
        return {};
    };

    CursorPath next;
    if (right) {
        if (offset < text->GetText().size()) {
            next = Concat(linePath, {child, text->NextCharacter(offset)});
        } else if (child + 2 < line->ChildCount()) {
            const CursorPath slot = anchorSlot(child + 1);
            next = slot.empty() ? Concat(linePath, {child + 2, 0})
                                : Concat(slot, NodeAt(slot)->FirstPosition());
        } else if (depths.size() == 1) {
            if (row + 1 < content.size()) {
                next = JoinPath({row + 1}, content[row + 1]->FirstPosition());
            }
        } else {
            // End of a slot: step out to just after the object.
            const std::size_t outer = depths[depths.size() - 2];
            next = Concat(Prefix(focus, outer), {focus[outer] + 1, 0});
        }
    } else {
        if (offset > 0) {
            next = Concat(linePath, {child, text->PreviousCharacter(offset)});
        } else if (child >= 2) {
            const CursorPath slot = anchorSlot(child - 1);
            next = slot.empty() ? Concat(linePath, {child - 2, TextLength(*line, child - 2)})
                                : Concat(slot, NodeAt(slot)->LastPosition());
        } else if (depths.size() == 1) {
            if (row > 0) {
                next = JoinPath({row - 1}, content[row - 1]->LastPosition());
            }
        } else {
            const std::size_t outer = depths[depths.size() - 2];
            const auto* outerLine = dynamic_cast<const LineNode*>(NodeAt(Prefix(focus, outer)));
            const std::size_t object = focus[outer];
            if (outerLine && object > 0) {
                next = Concat(Prefix(focus, outer),
                              {object - 1, TextLength(*outerLine, object - 1)});
            }
        }
    }
    if (next.empty() || next == cursorPos) {
        return false;
    }
    cursorPos = std::move(next);
    goalX_.reset();
    return true;
}
