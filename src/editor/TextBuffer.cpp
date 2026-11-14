// TextBuffer: editing plain text (cursor keys, typing, deleting, undo,
// clipboard). Used by FileText and by TextInput boxes in the UI.
//
// Every change to the text goes through Replace(), which also records it for
// undo as "at this position, this text was replaced by that text". Undo puts
// the removed text back; redo puts the inserted text back. Typing (and
// Backspace/Delete on single characters) in one place joins the previous step
// until a space is typed or the cursor moves, so undo goes a word at a time.

#include "TextBuffer.h"

#include "raylib.h"
#include "ui/shortcuts.h"

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

void TextBuffer::ClampPositions() {
    cursorPos = std::min(cursorPos, text.size());
    selectionStart = std::min(selectionStart, text.size());
}

void TextBuffer::Reset(std::string newText) {
    text = std::move(newText);
    cursorPos = 0;
    selectionStart = 0;
    goalX_.reset();
    undo_.clear();
    redo_.clear();
    groupOpen_ = false;
    TextChanged();
}

bool TextBuffer::PressKey(int key, bool shift, const LineMeasure* measure) {
    ClampPositions();
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
        if (singleLine) {
            return false;  // the owner's (e.g. moving through a result list)
        }
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
        if (singleLine) {
            return false;  // the owner's (e.g. submit)
        }
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
bool TextBuffer::MoveVertically(bool down, bool shift, const LineMeasure* measure) {
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

void TextBuffer::SelectAll() {
    selectionStart = 0;
    cursorPos = text.size();
    goalX_.reset();
}

// --- Editing ---------------------------------------------------------------------

bool TextBuffer::TypeCharacter(char32_t codepoint) {
    if (codepoint < 32 || codepoint == 127) {
        return false;
    }
    ClampPositions();
    // A space ends the word, so each word (with its space) undoes in one go.
    Replace(std::min(selectionStart, cursorPos), std::max(selectionStart, cursorPos),
            EncodeUtf8(codepoint), EditKind::Typing, codepoint == U' ');
    return true;
}

std::string TextBuffer::SelectedText() const {
    const auto start = std::min({selectionStart, cursorPos, text.size()});
    const auto end = std::min(std::max(selectionStart, cursorPos), text.size());
    return text.substr(start, end - start);
}

bool TextBuffer::PasteText(std::string pasted) {
    if (singleLine) {
        // Keep it on one line: each line break becomes a space.
        std::string flat;
        for (std::size_t i = 0; i < pasted.size(); ++i) {
            if (pasted[i] == '\r' && i + 1 < pasted.size() && pasted[i + 1] == '\n') {
                continue;
            }
            flat += pasted[i] == '\n' || pasted[i] == '\r' ? ' ' : pasted[i];
        }
        pasted = std::move(flat);
    }
    if (pasted.empty()) {
        return false;
    }
    ClampPositions();
    Replace(std::min(selectionStart, cursorPos), std::max(selectionStart, cursorPos),
            std::move(pasted), EditKind::Other);
    return true;
}

bool TextBuffer::DeleteSelection() {
    ClampPositions();
    if (selectionStart == cursorPos) {
        return false;
    }
    Replace(std::min(selectionStart, cursorPos), std::max(selectionStart, cursorPos), {},
            EditKind::Other);
    return true;
}

void TextBuffer::Replace(std::size_t start, std::size_t end, std::string inserted, EditKind kind,
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
    TextChanged();
}

bool TextBuffer::Undo() {
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
    TextChanged();
    return true;
}

bool TextBuffer::Redo() {
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
    TextChanged();
    return true;
}

// --- Keyboard ------------------------------------------------------------------------

bool TextBuffer::HandleKeyboard(const LineMeasure* measure, const CharacterFilter& accept) {
    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    const bool shortcut = IsShortcutModifierDown();  // Command on macOS
    auto pressed = [](int key) { return IsKeyPressed(key) || IsKeyPressedRepeat(key); };
    // Shortcut letters must not also be typed (GetCharPressed can report them).
    auto dropTypedCharacters = [] {
        while (GetCharPressed() > 0) {
        }
    };

    if (shortcut) {
        if (IsKeyPressed(KEY_A)) {
            SelectAll();
            dropTypedCharacters();
            return true;
        }
        if (IsKeyPressed(KEY_C)) {
            if (const std::string selected = SelectedText(); !selected.empty()) {
                SetClipboardText(selected.c_str());
            }
            dropTypedCharacters();
            return false;
        }
        if (IsKeyPressed(KEY_X)) {
            const std::string selected = SelectedText();
            if (!selected.empty()) {
                SetClipboardText(selected.c_str());
            }
            dropTypedCharacters();
            return !selected.empty() && DeleteSelection();
        }
        if (IsKeyPressed(KEY_V)) {
            const char* pasted = GetClipboardText();
            dropTypedCharacters();
            return pasted && PasteText(pasted);
        }
        // Z undoes; Y or Shift+Z redoes. Held keys repeat.
        if (pressed(KEY_Z)) {
            dropTypedCharacters();
            return shift ? Redo() : Undo();
        }
        if (pressed(KEY_Y)) {
            dropTypedCharacters();
            return Redo();
        }
    }

    bool changed = false;
    for (const int key :
         {KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN, KEY_BACKSPACE, KEY_DELETE, KEY_ENTER}) {
        if (pressed(key)) {
            changed |= PressKey(key, shift, measure);
        }
    }
    if (pressed(KEY_KP_ENTER)) {
        changed |= PressKey(KEY_ENTER, shift, measure);
    }
    while (const int codepoint = GetCharPressed()) {
        const auto character = static_cast<char32_t>(codepoint);
        if (!accept || accept(character)) {
            changed |= TypeCharacter(character);
        }
    }
    return changed;
}
