// Editing plain text files (FileText): cursor keys, typing, deleting, undo.
//
// Every change to the text goes through Replace(), which also records it for
// undo as "at this position, this text was replaced by that text". Undo puts
// the removed text back; redo puts the inserted text back. Typing (and
// Backspace/Delete on single characters) in one place joins the previous step
// until a space is typed or the cursor moves, so undo goes a word at a time.

#include "fileText.h"

#include "raylib.h"

#include <algorithm>
#include <utility>

namespace {
// Byte offset of the UTF-8 character before `position`.
std::size_t PreviousCharacter(const std::string& text, std::size_t position) {
    if (position == 0) {
        return 0;
    }
    --position;
    while (position > 0 && (static_cast<unsigned char>(text[position]) & 0xC0) == 0x80) {
        --position;
    }
    return position;
}

// Byte offset of the UTF-8 character after `position`.
std::size_t NextCharacter(const std::string& text, std::size_t position) {
    if (position >= text.size()) {
        return text.size();
    }
    ++position;
    while (position < text.size() && (static_cast<unsigned char>(text[position]) & 0xC0) == 0x80) {
        ++position;
    }
    return position;
}

std::size_t LineStart(const std::string& text, std::size_t position) {
    const auto newline = position == 0 ? std::string::npos : text.rfind('\n', position - 1);
    return newline == std::string::npos ? 0 : newline + 1;
}

std::size_t LineEnd(const std::string& text, std::size_t position) {
    const auto newline = text.find('\n', position);
    return newline == std::string::npos ? text.size() : newline;
}

std::string EncodeUtf8(char32_t codepoint) {
    int bytes = 0;
    const char* encoded = CodepointToUTF8(static_cast<int>(codepoint), &bytes);
    return std::string(encoded, static_cast<std::size_t>(bytes));
}
}  // namespace

// --- Cursor movement ----------------------------------------------------------

bool FileText::PressKey(int key, bool shift, const LineMeasure* measure) {
    cursorPos = std::min(cursorPos, text.size());
    selectionStart = std::min(selectionStart, text.size());
    const std::size_t start = std::min(selectionStart, cursorPos);
    const std::size_t end = std::max(selectionStart, cursorPos);
    const bool selected = start != end;

    switch (key) {
    case KEY_LEFT:
    case KEY_RIGHT: {
        const std::size_t previousCursor = cursorPos;
        const std::size_t previousAnchor = selectionStart;
        if (!shift && selected) {
            cursorPos = key == KEY_LEFT ? start : end;  // collapse to that end
        } else {
            cursorPos = key == KEY_LEFT ? PreviousCharacter(text, cursorPos)
                                        : NextCharacter(text, cursorPos);
        }
        if (!shift) {
            selectionStart = cursorPos;
        }
        goalX_.reset();
        return cursorPos != previousCursor || selectionStart != previousAnchor;
    }
    case KEY_UP:
    case KEY_DOWN:
        return MoveVertically(key == KEY_DOWN, shift, measure);
    case KEY_BACKSPACE:
        if (DeleteSelection()) {
            return true;
        }
        if (cursorPos == 0) {
            return false;
        }
        Replace(PreviousCharacter(text, cursorPos), cursorPos, {}, EditKind::Deleting);
        return true;
    case KEY_DELETE:
        if (DeleteSelection()) {
            return true;
        }
        if (cursorPos == text.size()) {
            return false;
        }
        Replace(cursorPos, NextCharacter(text, cursorPos), {}, EditKind::Deleting);
        return true;
    case KEY_ENTER:
        Replace(start, end, "\n", EditKind::Other);
        return true;
    default:
        return false;
    }
}

// Up/Down go to the character nearest the same x on the line above/below.
// The x is kept across repeated presses, so passing through a short line
// doesn't lose the column. On the first line Up goes to the start of the
// text, and on the last line Down goes to the end, as in most editors.
bool FileText::MoveVertically(bool down, bool shift, const LineMeasure* measure) {
    auto width = [&](std::size_t from, std::size_t to) -> float {
        if (measure) {
            return (*measure)(std::string_view(text).substr(from, to - from));
        }
        float characters = 0;  // no fonts (tests): columns in characters
        for (std::size_t i = from; i < to; i = NextCharacter(text, i)) {
            ++characters;
        }
        return characters;
    };

    const std::size_t previousCursor = cursorPos;
    const std::size_t previousAnchor = selectionStart;
    if (!goalX_ || goalPos_ != cursorPos) {
        goalX_ = width(LineStart(text, cursorPos), cursorPos);
    }

    // Position in the line starting at `lineStart` nearest to goalX_.
    auto nearest = [&](std::size_t lineStart) {
        const std::size_t lineEnd = LineEnd(text, lineStart);
        std::size_t position = lineStart;
        float x = 0;
        while (position < lineEnd) {
            const std::size_t next = NextCharacter(text, position);
            const float characterWidth = width(position, next);
            if (*goalX_ < x + characterWidth / 2) {
                break;
            }
            x += characterWidth;
            position = next;
        }
        return position;
    };

    if (down) {
        const std::size_t lineEnd = LineEnd(text, cursorPos);
        cursorPos = lineEnd == text.size() ? text.size() : nearest(lineEnd + 1);
    } else {
        const std::size_t lineStart = LineStart(text, cursorPos);
        cursorPos = lineStart == 0 ? 0 : nearest(LineStart(text, lineStart - 1));
    }
    if (!shift) {
        selectionStart = cursorPos;
    }
    goalPos_ = cursorPos;
    return cursorPos != previousCursor || selectionStart != previousAnchor;
}

void FileText::SelectAll() {
    selectionStart = 0;
    cursorPos = text.size();
    goalX_.reset();
}

// --- Editing ---------------------------------------------------------------------

bool FileText::TypeCharacter(char32_t codepoint) {
    if (codepoint < 32 || codepoint == 127) {
        return false;
    }
    cursorPos = std::min(cursorPos, text.size());
    selectionStart = std::min(selectionStart, text.size());
    // A space ends the word, so each word (with its space) undoes in one go.
    Replace(std::min(selectionStart, cursorPos), std::max(selectionStart, cursorPos),
            EncodeUtf8(codepoint), EditKind::Typing, codepoint == U' ');
    return true;
}

bool FileText::DeleteSelection() {
    if (selectionStart == cursorPos) {
        return false;
    }
    Replace(std::min(selectionStart, cursorPos), std::max(selectionStart, cursorPos), {},
            EditKind::Other);
    return true;
}

void FileText::Replace(std::size_t start, std::size_t end, std::string inserted, EditKind kind,
                       bool endsGroup) {
    UndoStep step;
    step.position = start;
    step.removed = text.substr(start, end - start);
    step.inserted = inserted;
    step.cursorBefore = cursorPos;
    step.anchorBefore = selectionStart;
    step.kind = kind;

    text.replace(start, end - start, inserted);
    cursorPos = start + inserted.size();
    selectionStart = cursorPos;
    goalX_.reset();
    step.cursorAfter = cursorPos;
    step.anchorAfter = selectionStart;
    redo_.clear();

    // Join the previous step if this carries straight on from it.
    bool joined = false;
    if (groupOpen_ && !undo_.empty() && kind != EditKind::Other) {
        UndoStep& last = undo_.back();
        const bool continues = last.kind == kind && last.cursorAfter == step.cursorBefore &&
                               step.anchorBefore == step.cursorBefore;
        if (continues && kind == EditKind::Typing && step.removed.empty() &&
            last.position + last.inserted.size() == start) {
            last.inserted += step.inserted;
            joined = true;
        } else if (continues && kind == EditKind::Deleting && last.inserted.empty()) {
            if (start + step.removed.size() == last.position) {  // Backspace
                last.removed.insert(0, step.removed);
                last.position = start;
                joined = true;
            } else if (start == last.position) {  // Delete
                last.removed += step.removed;
                joined = true;
            }
        }
        if (joined) {
            last.cursorAfter = step.cursorAfter;
            last.anchorAfter = step.anchorAfter;
        }
    }
    if (!joined) {
        undo_.push_back(std::move(step));
        if (undo_.size() > maxUndoSteps) {
            undo_.pop_front();
        }
    }
    groupOpen_ = kind != EditKind::Other && !endsGroup;
    UpdateModified();
}

void FileText::UpdateModified() {
    modified = !savedText_ || text != *savedText_;
}

bool FileText::Undo() {
    if (undo_.empty()) {
        return false;
    }
    UndoStep step = std::move(undo_.back());
    undo_.pop_back();
    text.replace(step.position, step.inserted.size(), step.removed);
    cursorPos = step.cursorBefore;
    selectionStart = step.anchorBefore;
    redo_.push_back(std::move(step));
    groupOpen_ = false;
    goalX_.reset();
    UpdateModified();
    return true;
}

bool FileText::Redo() {
    if (redo_.empty()) {
        return false;
    }
    UndoStep step = std::move(redo_.back());
    redo_.pop_back();
    text.replace(step.position, step.removed.size(), step.inserted);
    cursorPos = step.cursorAfter;
    selectionStart = step.anchorAfter;
    undo_.push_back(std::move(step));
    groupOpen_ = false;
    goalX_.reset();
    UpdateModified();
    return true;
}
