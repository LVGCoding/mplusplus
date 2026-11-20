// Editing plain text files: arrow keys (including Up/Down), typing, deleting,
// and undo/redo. Runs without a window; Up/Down keep the column in characters.

#include "editor/fileText.h"
#include "raylib.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <string>

namespace {
FileText MakeDocument(const std::string& text,
                      std::size_t cursor = 0) {
    FileText file;
    file.text = text;
    file.cursorPos = cursor;
    file.selectionStart = cursor;
    return file;
}

bool PressKey(FileText& file,
              int key,
              bool shift = false) {
    return file.PressKey(key, shift, nullptr);
}

// These fixtures type ASCII bytes; UTF-8 cases exercise cursor movement or paste separately.
void TypeAscii(FileText& file,
               const std::string& text) {
    for (const char character : text) {
        file.TypeCharacter(static_cast<char32_t>(character));
    }
}
}  // namespace

// Up and Down 

TEST(TextKeys,
     UpAndDownKeepTheColumn) {
    auto file = MakeDocument("abcdef\nxy\nlmnopq", 4);  // after "abcd"
    ASSERT_TRUE(PressKey(file, KEY_DOWN));
    EXPECT_EQ(file.cursorPos, 9u);  // end of the short line "xy"
    ASSERT_TRUE(PressKey(file, KEY_DOWN));
    EXPECT_EQ(file.cursorPos, 14u);  // column 4 again, after "lmno"
    ASSERT_TRUE(PressKey(file, KEY_UP));
    ASSERT_TRUE(PressKey(file, KEY_UP));
    EXPECT_EQ(file.cursorPos, 4u);
}

TEST(TextKeys,
     UpOnTheFirstLineGoesToTheStartAndDownOnTheLastToTheEnd) {
    auto file = MakeDocument("abc\ndef", 2);
    ASSERT_TRUE(PressKey(file, KEY_UP));
    EXPECT_EQ(file.cursorPos, 0u);
    file.cursorPos = file.selectionStart = 5;
    ASSERT_TRUE(PressKey(file, KEY_DOWN));
    EXPECT_EQ(file.cursorPos, 7u);
    EXPECT_FALSE(PressKey(file, KEY_DOWN));
}

TEST(TextKeys,
     UpAndDownThroughBlankLines) {
    auto file = MakeDocument("abc\n\nabc", 2);
    PressKey(file, KEY_DOWN);
    EXPECT_EQ(file.cursorPos, 4u);  // the blank line
    PressKey(file, KEY_DOWN);
    EXPECT_EQ(file.cursorPos, 7u);  // column 2 remembered
}

TEST(TextKeys,
     ColumnsCountCharactersNotBytes) {
    auto file = MakeDocument("日本語\nabc", 6);  // after two 3-byte characters
    PressKey(file, KEY_DOWN);
    EXPECT_EQ(file.cursorPos, 12u);  // after "ab"
    PressKey(file, KEY_UP);
    EXPECT_EQ(file.cursorPos, 6u);
}

TEST(TextKeys,
     ShiftUpAndDownSelectLines) {
    auto file = MakeDocument("abc\ndef\nghi", 5);
    PressKey(file, KEY_DOWN, true);
    EXPECT_EQ(file.selectionStart, 5u);
    EXPECT_EQ(file.cursorPos, 9u);
    PressKey(file, KEY_DOWN);
    EXPECT_EQ(file.selectionStart, file.cursorPos);
}

TEST(TextKeys,
     LeftAndRightCollapseTheSelection) {
    auto file = MakeDocument("abcdef", 1);
    PressKey(file, KEY_RIGHT, true);
    PressKey(file, KEY_RIGHT, true);
    PressKey(file, KEY_LEFT);
    EXPECT_EQ(file.cursorPos, 1u);
    EXPECT_EQ(file.selectionStart, 1u);
}

TEST(TextKeys,
     LeftAndRightResetTheColumn) {
    auto file = MakeDocument("abcdef\nabcdef", 4);
    PressKey(file, KEY_LEFT);
    PressKey(file, KEY_DOWN);
    EXPECT_EQ(file.cursorPos, 10u);  // column 3, not 4
}

// Editing 

TEST(TextEdit,
     TypingReplacesTheSelection) {
    auto file = MakeDocument("abcd", 1);
    PressKey(file, KEY_RIGHT, true);
    PressKey(file, KEY_RIGHT, true);
    TypeAscii(file, "X");
    EXPECT_EQ(file.text, "aXd");
    EXPECT_EQ(file.cursorPos, 2u);
    EXPECT_TRUE(file.modified);
}

TEST(TextEdit,
     BackspaceDeleteAndEnter) {
    auto file = MakeDocument("abc", 2);
    PressKey(file, KEY_BACKSPACE);
    EXPECT_EQ(file.text, "ac");
    PressKey(file, KEY_DELETE);
    EXPECT_EQ(file.text, "a");
    PressKey(file, KEY_ENTER);
    EXPECT_EQ(file.text, "a\n");
    EXPECT_EQ(file.cursorPos, 2u);
}

TEST(TextEdit,
     SelectAll) {
    auto file = MakeDocument("one\ntwo");
    file.SelectAll();
    TypeAscii(file, "x");
    EXPECT_EQ(file.text, "x");
}

// Undo and redo 

TEST(TextHistory,
     TypingUndoesAWordAtATime) {
    auto file = MakeDocument("");
    TypeAscii(file, "hello world");
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

TEST(TextHistory,
     BackspaceAndDeleteGroup) {
    auto file = MakeDocument("abcdef", 3);
    PressKey(file, KEY_BACKSPACE);
    PressKey(file, KEY_BACKSPACE);
    ASSERT_EQ(file.text, "adef");
    ASSERT_TRUE(file.Undo());
    EXPECT_EQ(file.text, "abcdef");
    EXPECT_EQ(file.cursorPos, 3u);

    PressKey(file, KEY_DELETE);
    PressKey(file, KEY_DELETE);
    ASSERT_EQ(file.text, "abcf");
    ASSERT_TRUE(file.Undo());
    EXPECT_EQ(file.text, "abcdef");
    EXPECT_FALSE(file.CanUndo());
}

TEST(TextHistory,
     MovingTheCursorStartsANewStep) {
    auto file = MakeDocument("ac", 1);
    TypeAscii(file, "b");
    PressKey(file, KEY_RIGHT);
    TypeAscii(file, "d");
    ASSERT_TRUE(file.Undo());
    EXPECT_EQ(file.text, "abc");
}

TEST(TextHistory,
     ReplacedSelectionComesBackSelected) {
    auto file = MakeDocument("abcd", 1);
    PressKey(file, KEY_RIGHT, true);
    PressKey(file, KEY_RIGHT, true);
    TypeAscii(file, "XY");
    ASSERT_EQ(file.text, "aXYd");
    ASSERT_TRUE(file.Undo());  // the replacement and the typing after it
    EXPECT_EQ(file.text, "abcd");
    EXPECT_EQ(file.selectionStart, 1u);
    EXPECT_EQ(file.cursorPos, 3u);
}

TEST(TextHistory,
     EnterIsItsOwnStepAndNewEditsClearRedo) {
    auto file = MakeDocument("");
    TypeAscii(file, "ab");
    PressKey(file, KEY_ENTER);
    TypeAscii(file, "cd");
    ASSERT_TRUE(file.Undo());
    EXPECT_EQ(file.text, "ab\n");
    ASSERT_TRUE(file.Undo());
    EXPECT_EQ(file.text, "ab");
    TypeAscii(file, "x");
    EXPECT_FALSE(file.CanRedo());
}

TEST(TextHistory,
     UndoingBackToTheSavedTextClearsModified) {
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
    TypeAscii(file, "d");
    EXPECT_TRUE(file.modified);
    file.Undo();
    EXPECT_FALSE(file.modified);
    std::filesystem::remove(path);
}
