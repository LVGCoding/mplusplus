// Undo and redo in M++ documents. Runs without a window.

#include "editor/FileMpp.h"
#include "math/FracNode.h"
#include "math/LineNode.h"
#include "raylib.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <memory>
#include <string>

namespace {
FileMpp MakeDocument(const std::string& source,
                     CursorPath cursor = {}) {
    FileMpp file;
    file.SetSource(source);
    if (!cursor.empty()) {
        file.MoveCursorTo(std::move(cursor));
    }
    return file;
}

bool PressKey(FileMpp& file,
              int key,
              bool shift = false) {
    return file.PressKey(key, shift, nullptr);
}

// These fixtures type ASCII bytes; UTF-8 cases exercise cursor movement or paste separately.
void TypeAscii(FileMpp& file,
               const std::string& text) {
    for (const char character : text) {
        file.TypeCharacter(static_cast<char32_t>(character));
    }
}
}  // namespace

TEST(History,
     NothingToUndoAtFirst) {
    auto file = MakeDocument("abc");
    EXPECT_FALSE(file.CanUndo());
    EXPECT_FALSE(file.Undo());
    EXPECT_FALSE(file.Redo());
}

TEST(History,
     TypingUndoesAWordAtATime) {
    auto file = MakeDocument("");
    TypeAscii(file, "hello world");
    ASSERT_TRUE(file.Undo());
    EXPECT_EQ(file.ContentText(), "hello ");
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 0, 6}));
    ASSERT_TRUE(file.Undo());
    EXPECT_EQ(file.ContentText(), "");
    EXPECT_FALSE(file.CanUndo());
    ASSERT_TRUE(file.Redo());
    EXPECT_EQ(file.ContentText(), "hello ");
    ASSERT_TRUE(file.Redo());
    EXPECT_EQ(file.ContentText(), "hello world");
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 0, 11}));
    EXPECT_EQ(file.source, "hello world");
}

TEST(History,
     MovingTheCursorStartsANewStep) {
    auto file = MakeDocument("ac", {0, 0, 1});
    TypeAscii(file, "b");
    PressKey(file, KEY_RIGHT);
    TypeAscii(file, "d");
    ASSERT_TRUE(file.Undo());
    EXPECT_EQ(file.ContentText(), "abc");
    ASSERT_TRUE(file.Undo());
    EXPECT_EQ(file.ContentText(), "ac");
}

TEST(History,
     BackspacesGroupLikeTyping) {
    auto file = MakeDocument("abcd", {0, 0, 4});
    PressKey(file, KEY_BACKSPACE);
    PressKey(file, KEY_BACKSPACE);
    EXPECT_EQ(file.ContentText(), "ab");
    ASSERT_TRUE(file.Undo());
    EXPECT_EQ(file.ContentText(), "abcd");
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 0, 4}));
    EXPECT_FALSE(file.CanUndo());
}

TEST(History,
     SplittingAndJoiningRows) {
    auto file = MakeDocument("abcd\nxy", {0, 0, 2});
    PressKey(file, KEY_ENTER);
    ASSERT_EQ(file.content.size(), 3u);
    PressKey(file, KEY_BACKSPACE);  // joins them back
    PressKey(file, KEY_ENTER);
    ASSERT_TRUE(file.Undo());
    EXPECT_EQ(file.ContentText(), "abcd\nxy");
    ASSERT_TRUE(file.Undo());
    EXPECT_EQ(file.ContentText(), "ab\ncd\nxy");
    EXPECT_EQ(file.cursorPos, (CursorPath{1, 0, 0}));
    ASSERT_TRUE(file.Undo());
    EXPECT_EQ(file.ContentText(), "abcd\nxy");
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 0, 2}));
}

TEST(History,
     DeletedObjectComesBackAsItWas) {
    auto file = MakeDocument("a\\frac{1}{2}b", {0, 2, 0});
    const auto fracId = file.content[0]->GetChild(1)->GetId();
    PressKey(file, KEY_BACKSPACE);  // mark
    PressKey(file, KEY_BACKSPACE);  // delete
    ASSERT_EQ(file.ContentText(), "ab");
    ASSERT_TRUE(file.Undo());
    EXPECT_EQ(file.ContentText(), "a\\frac{1}{2}b");
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 2, 0}));
    // Same node ids, so it lays out exactly where it was.
    EXPECT_EQ(file.content[0]->GetChild(1)->GetId(), fracId);
    EXPECT_FALSE(file.GetPendingDelete());
}

TEST(History,
     SelectionDeletedAcrossRowsComesBackSelected) {
    auto file = MakeDocument("ab\ncd\nef", {0, 0, 1});
    file.PressKey(KEY_DOWN, true, nullptr);
    file.PressKey(KEY_DOWN, true, nullptr);
    file.PressKey(KEY_RIGHT, true, nullptr);
    TypeAscii(file, "X");
    ASSERT_EQ(file.ContentText(), "aXf");
    ASSERT_TRUE(file.Undo());
    EXPECT_EQ(file.ContentText(), "ab\ncd\nef");
    EXPECT_EQ(file.selectionStart, (CursorPath{0, 0, 1}));
    EXPECT_EQ(file.cursorPos, (CursorPath{2, 0, 1}));
}

TEST(History,
     MatrixRowInsertAndInsertedObjects) {
    auto file = MakeDocument("x", {0, 0, 1});
    ASSERT_TRUE(file.InsertNode(
        std::make_unique<FracNode>(std::make_unique<LineNode>(), std::make_unique<LineNode>())));
    TypeAscii(file, "1");
    ASSERT_TRUE(file.Undo());
    EXPECT_EQ(file.ContentText(), "x\\frac{}{}");
    ASSERT_TRUE(file.Undo());
    EXPECT_EQ(file.ContentText(), "x");

    auto matrix = MakeDocument("\\begin{matrix}a&b\\\\c&d\\end{matrix}", {0, 1, 0, 0, 0, 1});
    PressKey(matrix, KEY_ENTER);
    PressKey(matrix, KEY_DOWN);
    PressKey(matrix, KEY_ENTER);  // row below
    ASSERT_EQ(matrix.ContentText(), "\\begin{matrix}a&b\\\\&\\\\c&d\\end{matrix}");
    ASSERT_TRUE(matrix.Undo());
    EXPECT_EQ(matrix.ContentText(), "\\begin{matrix}a&b\\\\c&d\\end{matrix}");
    EXPECT_EQ(matrix.cursorPos, (CursorPath{0, 1, 0, 0, 0, 1}));
}

TEST(History,
     NewEditClearsRedo) {
    auto file = MakeDocument("");
    TypeAscii(file, "ab");
    file.Undo();
    ASSERT_TRUE(file.CanRedo());
    TypeAscii(file, "c");
    EXPECT_FALSE(file.CanRedo());
    EXPECT_EQ(file.ContentText(), "c");
}

TEST(History,
     UntouchedRowsAreNotReplaced) {
    std::string source;
    for (int i = 0; i < 100; ++i) {
        source += "row \\frac{" + std::to_string(i) + "}{2}\n";
    }
    auto file = MakeDocument(source, {50, 0, 4});
    const LineNode* far = file.content[10].get();
    const LineNode* next = file.content[51].get();
    TypeAscii(file, "x");
    ASSERT_TRUE(file.Undo());
    ASSERT_TRUE(file.Redo());
    ASSERT_TRUE(file.Undo());
    EXPECT_EQ(file.content[10].get(), far);
    EXPECT_EQ(file.content[51].get(), next);
    EXPECT_EQ(file.ContentText(), source);
}

TEST(History,
     UndoingBackToTheSavedTextClearsModified) {
    const auto path = std::filesystem::temp_directory_path() / "mplusplus_history_test.mpp";
    std::string error;
    {
        FileMpp writer;
        writer.SetSource("abc");
        ASSERT_TRUE(writer.Save(path, error)) << error;
    }
    FileMpp file;
    ASSERT_TRUE(file.Load(path, error)) << error;
    file.MoveCursorTo({0, 0, 3});
    TypeAscii(file, "d");
    EXPECT_TRUE(file.modified);
    file.Undo();
    EXPECT_FALSE(file.modified);
    file.Redo();
    EXPECT_TRUE(file.modified);
    std::filesystem::remove(path);
}

TEST(History,
     LoadingClearsHistory) {
    auto file = MakeDocument("");
    TypeAscii(file, "a");
    file.SetSource("b");
    EXPECT_FALSE(file.CanUndo());
}
