// Editing plain text files: arrow keys (including Up/Down), typing, deleting,
// and undo/redo. Runs without a window; Up/Down keep the column in characters.

#include "editor/fileText.h"
#include "raylib.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

namespace {
FileText Doc(const std::string& text, std::size_t cursor = 0) {
    FileText file;
    file.text = text;
    file.cursorPos = cursor;
    file.selectionStart = cursor;
    return file;
}

bool Press(FileText& file, int key, bool shift = false) {
    return file.PressKey(key, shift, nullptr);
}

void Type(FileText& file, const std::string& text) {
    for (const char c : text) {
        file.TypeCharacter(static_cast<char32_t>(c));
    }
}
}  // namespace

// --- Up and Down ---------------------------------------------------------------------

TEST(TextKeys, UpAndDownKeepTheColumn) {
    auto file = Doc("abcdef\nxy\nlmnopq", 4);  // after "abcd"
    ASSERT_TRUE(Press(file, KEY_DOWN));
    EXPECT_EQ(file.cursorPos, 9u);  // end of the short line "xy"
    ASSERT_TRUE(Press(file, KEY_DOWN));
    EXPECT_EQ(file.cursorPos, 14u);  // column 4 again, after "lmno"
    ASSERT_TRUE(Press(file, KEY_UP));
    ASSERT_TRUE(Press(file, KEY_UP));
    EXPECT_EQ(file.cursorPos, 4u);
}

TEST(TextKeys, UpOnTheFirstLineGoesToTheStartAndDownOnTheLastToTheEnd) {
    auto file = Doc("abc\ndef", 2);
    ASSERT_TRUE(Press(file, KEY_UP));
    EXPECT_EQ(file.cursorPos, 0u);
    file.cursorPos = file.selectionStart = 5;
    ASSERT_TRUE(Press(file, KEY_DOWN));
    EXPECT_EQ(file.cursorPos, 7u);
    EXPECT_FALSE(Press(file, KEY_DOWN));
}

TEST(TextKeys, UpAndDownThroughBlankLines) {
    auto file = Doc("abc\n\nabc", 2);
    Press(file, KEY_DOWN);
    EXPECT_EQ(file.cursorPos, 4u);  // the blank line
    Press(file, KEY_DOWN);
    EXPECT_EQ(file.cursorPos, 7u);  // column 2 remembered
}

TEST(TextKeys, ColumnsCountCharactersNotBytes) {
    auto file = Doc("日本語\nabc", 6);  // after two 3-byte characters
    Press(file, KEY_DOWN);
    EXPECT_EQ(file.cursorPos, 12u);  // after "ab"
    Press(file, KEY_UP);
    EXPECT_EQ(file.cursorPos, 6u);
}

TEST(TextKeys, ShiftUpAndDownSelectLines) {
    auto file = Doc("abc\ndef\nghi", 5);
    Press(file, KEY_DOWN, true);
    EXPECT_EQ(file.selectionStart, 5u);
    EXPECT_EQ(file.cursorPos, 9u);
    Press(file, KEY_DOWN);
    EXPECT_EQ(file.selectionStart, file.cursorPos);
}

TEST(TextKeys, LeftAndRightCollapseTheSelection) {
    auto file = Doc("abcdef", 1);
    Press(file, KEY_RIGHT, true);
    Press(file, KEY_RIGHT, true);
    Press(file, KEY_LEFT);
    EXPECT_EQ(file.cursorPos, 1u);
    EXPECT_EQ(file.selectionStart, 1u);
}

TEST(TextKeys, LeftAndRightResetTheColumn) {
    auto file = Doc("abcdef\nabcdef", 4);
    Press(file, KEY_LEFT);
    Press(file, KEY_DOWN);
    EXPECT_EQ(file.cursorPos, 10u);  // column 3, not 4
}

// --- Editing -----------------------------------------------------------------------------

TEST(TextEdit, TypingReplacesTheSelection) {
    auto file = Doc("abcd", 1);
    Press(file, KEY_RIGHT, true);
    Press(file, KEY_RIGHT, true);
    Type(file, "X");
    EXPECT_EQ(file.text, "aXd");
    EXPECT_EQ(file.cursorPos, 2u);
    EXPECT_TRUE(file.modified);
}

TEST(TextEdit, BackspaceDeleteAndEnter) {
    auto file = Doc("abc", 2);
    Press(file, KEY_BACKSPACE);
    EXPECT_EQ(file.text, "ac");
    Press(file, KEY_DELETE);
    EXPECT_EQ(file.text, "a");
    Press(file, KEY_ENTER);
    EXPECT_EQ(file.text, "a\n");
    EXPECT_EQ(file.cursorPos, 2u);
}

TEST(TextEdit, SelectAll) {
    auto file = Doc("one\ntwo");
    file.SelectAll();
    Type(file, "x");
    EXPECT_EQ(file.text, "x");
}

// --- Undo and redo --------------------------------------------------------------------

TEST(TextHistory, TypingUndoesAWordAtATime) {
    auto file = Doc("");
    Type(file, "hello world");
    ASSERT_TRUE(file.Undo());
    EXPECT_EQ(file.text, "hello ");
    ASSERT_TRUE(file.Undo());
    EXPECT_EQ(file.text, "");
    EXPECT_FALSE(file.Undo());
    ASSERT_TRUE(file.Redo());
    ASSERT_TRUE(file.Redo());
    EXPECT_EQ(file.text, "hello world");
    EXPECT_EQ(file.cursorPos, 11u);
}

TEST(TextHistory, BackspaceAndDeleteGroup) {
    auto file = Doc("abcdef", 3);
    Press(file, KEY_BACKSPACE);
    Press(file, KEY_BACKSPACE);
    ASSERT_EQ(file.text, "adef");
    ASSERT_TRUE(file.Undo());
    EXPECT_EQ(file.text, "abcdef");
    EXPECT_EQ(file.cursorPos, 3u);

    Press(file, KEY_DELETE);
    Press(file, KEY_DELETE);
    ASSERT_EQ(file.text, "abcf");
    ASSERT_TRUE(file.Undo());
    EXPECT_EQ(file.text, "abcdef");
    EXPECT_FALSE(file.CanUndo());
}

TEST(TextHistory, MovingTheCursorStartsANewStep) {
    auto file = Doc("ac", 1);
    Type(file, "b");
    Press(file, KEY_RIGHT);
    Type(file, "d");
    ASSERT_TRUE(file.Undo());
    EXPECT_EQ(file.text, "abc");
}

TEST(TextHistory, ReplacedSelectionComesBackSelected) {
    auto file = Doc("abcd", 1);
    Press(file, KEY_RIGHT, true);
    Press(file, KEY_RIGHT, true);
    Type(file, "XY");
    ASSERT_EQ(file.text, "aXYd");
    ASSERT_TRUE(file.Undo());  // the replacement and the typing after it
    EXPECT_EQ(file.text, "abcd");
    EXPECT_EQ(file.selectionStart, 1u);
    EXPECT_EQ(file.cursorPos, 3u);
}

TEST(TextHistory, EnterIsItsOwnStepAndNewEditsClearRedo) {
    auto file = Doc("");
    Type(file, "ab");
    Press(file, KEY_ENTER);
    Type(file, "cd");
    ASSERT_TRUE(file.Undo());
    EXPECT_EQ(file.text, "ab\n");
    ASSERT_TRUE(file.Undo());
    EXPECT_EQ(file.text, "ab");
    Type(file, "x");
    EXPECT_FALSE(file.CanRedo());
}

TEST(TextHistory, UndoingBackToTheSavedTextClearsModified) {
    const auto path = std::filesystem::temp_directory_path() / "mplusplus_text_history_test.txt";
    std::string error;
    {
        FileText writer;
        writer.text = "abc";
        ASSERT_TRUE(writer.Save(path, error)) << error;
    }
    FileText file;
    ASSERT_TRUE(file.Load(path, error)) << error;
    file.cursorPos = file.selectionStart = 3;
    Type(file, "d");
    EXPECT_TRUE(file.modified);
    file.Undo();
    EXPECT_FALSE(file.modified);
    std::filesystem::remove(path);
}
