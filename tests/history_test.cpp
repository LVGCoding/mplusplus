// Undo and redo in M++ documents. Runs without a window.

#include "editor/FileMpp.h"
#include "math/FracNode.h"
#include "math/LineNode.h"
#include "raylib.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>

namespace {
FileMpp Doc(const std::string& source, CursorPath cursor = {}) {
    FileMpp file;
    file.SetSource(source);
    if (!cursor.empty()) {
        file.MoveCursorTo(std::move(cursor));
    }
    return file;
}

bool Press(FileMpp& file, int key, bool shift = false) {
    return file.PressKey(key, shift, nullptr);
}

void Type(FileMpp& file, const std::string& text) {
    for (const char c : text) {
        file.TypeCharacter(static_cast<char32_t>(c));
    }
}
}  // namespace

TEST(History, NothingToUndoAtFirst) {
    auto file = Doc("abc");
    EXPECT_FALSE(file.CanUndo());
    EXPECT_FALSE(file.Undo());
    EXPECT_FALSE(file.Redo());
}

TEST(History, TypingUndoesAWordAtATime) {
    auto file = Doc("");
    Type(file, "hello world");
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

TEST(History, MovingTheCursorStartsANewStep) {
    auto file = Doc("ac", {0, 0, 1});
    Type(file, "b");
    Press(file, KEY_RIGHT);
    Type(file, "d");
    ASSERT_TRUE(file.Undo());
    EXPECT_EQ(file.ContentText(), "abc");
    ASSERT_TRUE(file.Undo());
    EXPECT_EQ(file.ContentText(), "ac");
}

TEST(History, BackspacesGroupLikeTyping) {
    auto file = Doc("abcd", {0, 0, 4});
    Press(file, KEY_BACKSPACE);
    Press(file, KEY_BACKSPACE);
    EXPECT_EQ(file.ContentText(), "ab");
    ASSERT_TRUE(file.Undo());
    EXPECT_EQ(file.ContentText(), "abcd");
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 0, 4}));
    EXPECT_FALSE(file.CanUndo());
}

TEST(History, SplittingAndJoiningRows) {
    auto file = Doc("abcd\nxy", {0, 0, 2});
    Press(file, KEY_ENTER);
    ASSERT_EQ(file.content.size(), 3u);
    Press(file, KEY_BACKSPACE);  // joins them back
    Press(file, KEY_ENTER);
    ASSERT_TRUE(file.Undo());
    EXPECT_EQ(file.ContentText(), "abcd\nxy");
    ASSERT_TRUE(file.Undo());
    EXPECT_EQ(file.ContentText(), "ab\ncd\nxy");
    EXPECT_EQ(file.cursorPos, (CursorPath{1, 0, 0}));
    ASSERT_TRUE(file.Undo());
    EXPECT_EQ(file.ContentText(), "abcd\nxy");
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 0, 2}));
}

TEST(History, DeletedObjectComesBackAsItWas) {
    auto file = Doc("a\\frac{1}{2}b", {0, 2, 0});
    const auto fracId = file.content[0]->GetChild(1)->GetId();
    Press(file, KEY_BACKSPACE);  // mark
    Press(file, KEY_BACKSPACE);  // delete
    ASSERT_EQ(file.ContentText(), "ab");
    ASSERT_TRUE(file.Undo());
    EXPECT_EQ(file.ContentText(), "a\\frac{1}{2}b");
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 2, 0}));
    // Same node ids, so it lays out exactly where it was.
    EXPECT_EQ(file.content[0]->GetChild(1)->GetId(), fracId);
    EXPECT_FALSE(file.GetPendingDelete());
}

TEST(History, SelectionDeletedAcrossRowsComesBackSelected) {
    auto file = Doc("ab\ncd\nef", {0, 0, 1});
    file.PressKey(KEY_DOWN, true, nullptr);
    file.PressKey(KEY_DOWN, true, nullptr);
    file.PressKey(KEY_RIGHT, true, nullptr);
    Type(file, "X");
    ASSERT_EQ(file.ContentText(), "aXf");
    ASSERT_TRUE(file.Undo());
    EXPECT_EQ(file.ContentText(), "ab\ncd\nef");
    EXPECT_EQ(file.selectionStart, (CursorPath{0, 0, 1}));
    EXPECT_EQ(file.cursorPos, (CursorPath{2, 0, 1}));
}

TEST(History, MatrixRowInsertAndInsertedObjects) {
    auto file = Doc("x", {0, 0, 1});
    ASSERT_TRUE(file.InsertNode(
        std::make_unique<FracNode>(std::make_unique<LineNode>(), std::make_unique<LineNode>())));
    Type(file, "1");
    ASSERT_TRUE(file.Undo());
    EXPECT_EQ(file.ContentText(), "x\\frac{}{}");
    ASSERT_TRUE(file.Undo());
    EXPECT_EQ(file.ContentText(), "x");

    auto matrix = Doc("\\begin{matrix}a&b\\\\c&d\\end{matrix}", {0, 1, 0, 0, 0, 1});
    Press(matrix, KEY_ENTER);
    Press(matrix, KEY_DOWN);
    Press(matrix, KEY_ENTER);  // row below
    ASSERT_EQ(matrix.ContentText(), "\\begin{matrix}a&b\\\\&\\\\c&d\\end{matrix}");
    ASSERT_TRUE(matrix.Undo());
    EXPECT_EQ(matrix.ContentText(), "\\begin{matrix}a&b\\\\c&d\\end{matrix}");
    EXPECT_EQ(matrix.cursorPos, (CursorPath{0, 1, 0, 0, 0, 1}));
}

TEST(History, NewEditClearsRedo) {
    auto file = Doc("");
    Type(file, "ab");
    file.Undo();
    ASSERT_TRUE(file.CanRedo());
    Type(file, "c");
    EXPECT_FALSE(file.CanRedo());
    EXPECT_EQ(file.ContentText(), "c");
}

TEST(History, UntouchedRowsAreNotReplaced) {
    std::string source;
    for (int i = 0; i < 100; ++i) {
        source += "row \\frac{" + std::to_string(i) + "}{2}\n";
    }
    auto file = Doc(source, {50, 0, 4});
    const LineNode* far = file.content[10].get();
    const LineNode* next = file.content[51].get();
    Type(file, "x");
    ASSERT_TRUE(file.Undo());
    ASSERT_TRUE(file.Redo());
    ASSERT_TRUE(file.Undo());
    EXPECT_EQ(file.content[10].get(), far);
    EXPECT_EQ(file.content[51].get(), next);
    EXPECT_EQ(file.ContentText(), source);
}

TEST(History, UndoingBackToTheSavedTextClearsModified) {
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
    Type(file, "d");
    EXPECT_TRUE(file.modified);
    file.Undo();
    EXPECT_FALSE(file.modified);
    file.Redo();
    EXPECT_TRUE(file.modified);
    std::filesystem::remove(path);
}

TEST(History, LoadingClearsHistory) {
    auto file = Doc("");
    Type(file, "a");
    file.SetSource("b");
    EXPECT_FALSE(file.CanUndo());
}
