// Selecting in M++ documents: Shift+arrows, what a selection covers, and
// replacing or deleting it. Runs without a window.

#include "editor/FileMpp.h"
#include "math/FracNode.h"
#include "math/LineNode.h"
#include "raylib.h"

#include <gtest/gtest.h>

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

bool Press(FileMpp& file, int key) {
    return file.PressKey(key, false, nullptr);
}

bool ShiftPress(FileMpp& file, int key, int times = 1) {
    bool changed = false;
    for (int i = 0; i < times; ++i) {
        changed |= file.PressKey(key, true, nullptr);
    }
    return changed;
}

void Type(FileMpp& file, const std::string& text) {
    for (const char c : text) {
        file.TypeCharacter(static_cast<char32_t>(c));
    }
}

void ExpectSelection(const FileMpp& file, const CursorPath& start, const CursorPath& end) {
    const auto selection = file.GetSelection();
    ASSERT_TRUE(selection.has_value());
    EXPECT_EQ(selection->start, start);
    EXPECT_EQ(selection->end, end);
}
}  // namespace

// --- Shift+arrows -----------------------------------------------------------------

TEST(SelectKeys, ShiftRightSelectsCharacters) {
    auto file = Doc("abcd", {0, 0, 1});
    EXPECT_FALSE(file.GetSelection());
    ASSERT_TRUE(ShiftPress(file, KEY_RIGHT, 2));
    ExpectSelection(file, {0, 0, 1}, {0, 0, 3});
    EXPECT_EQ(file.selectionStart, (CursorPath{0, 0, 1}));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 0, 3}));
}

TEST(SelectKeys, ShiftLeftThenRightShrinksBackToNothing) {
    auto file = Doc("abcd", {0, 0, 2});
    ShiftPress(file, KEY_LEFT, 2);
    ExpectSelection(file, {0, 0, 0}, {0, 0, 2});
    ShiftPress(file, KEY_RIGHT, 2);
    EXPECT_FALSE(file.GetSelection());
}

TEST(SelectKeys, ObjectsAreSelectedInOneStep) {
    auto file = Doc("a\\frac{1}{2}b", {0, 2, 0});  // just after the fraction
    ASSERT_TRUE(ShiftPress(file, KEY_LEFT));
    ExpectSelection(file, {0, 0, 1}, {0, 2, 0});
    ShiftPress(file, KEY_RIGHT);
    EXPECT_FALSE(file.GetSelection());
    ShiftPress(file, KEY_RIGHT);
    ExpectSelection(file, {0, 2, 0}, {0, 2, 1});
}

TEST(SelectKeys, LeavingASlotSelectsTheWholeObject) {
    auto file = Doc("\\frac{12}{3}x", {0, 1, 0, 0, 1});  // numerator, after "1"
    ShiftPress(file, KEY_RIGHT);
    ExpectSelection(file, {0, 1, 0, 0, 1}, {0, 1, 0, 0, 2});
    ShiftPress(file, KEY_RIGHT);  // out of the numerator
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 2, 0}));
    ExpectSelection(file, {0, 0, 0}, {0, 2, 0});
    // Stepping back goes into the slot holding the anchor again.
    ShiftPress(file, KEY_LEFT);
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 1, 0, 0, 2}));
    ExpectSelection(file, {0, 1, 0, 0, 1}, {0, 1, 0, 0, 2});
}

TEST(SelectKeys, EndsInDifferentSlotsSelectTheObject) {
    auto file = Doc("\\frac{1}{2}", {0, 1, 0, 0, 1});
    ASSERT_TRUE(ShiftPress(file, KEY_DOWN));  // into the denominator
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 1, 1, 0, 0}));
    ExpectSelection(file, {0, 0, 0}, {0, 2, 0});
}

TEST(SelectKeys, SelectionCrossesRows) {
    auto file = Doc("ab\ncd", {0, 0, 1});
    ShiftPress(file, KEY_RIGHT, 3);
    ExpectSelection(file, {0, 0, 1}, {1, 0, 1});
}

TEST(SelectKeys, LeftAndRightGoToTheEndsOfTheSelection) {
    auto file = Doc("a\\frac{1}{2}b", {0, 1, 0, 0, 0});
    ShiftPress(file, KEY_RIGHT, 2);  // out of the numerator: whole fraction
    ASSERT_TRUE(Press(file, KEY_LEFT));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 0, 1}));
    EXPECT_FALSE(file.GetSelection());

    file.MoveCursorTo({0, 0, 0});
    ShiftPress(file, KEY_RIGHT, 2);
    ASSERT_TRUE(Press(file, KEY_RIGHT));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 2, 0}));
    EXPECT_FALSE(file.GetSelection());
}

TEST(SelectKeys, EscapeClearsTheSelection) {
    auto file = Doc("abc", {0, 0, 0});
    ShiftPress(file, KEY_RIGHT);
    ASSERT_TRUE(Press(file, KEY_ESCAPE));
    EXPECT_FALSE(file.GetSelection());
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 0, 1}));
}

TEST(SelectKeys, SelectAll) {
    auto file = Doc("a\n\\frac{1}{2}\nb");
    file.SelectAll();
    ExpectSelection(file, {0, 0, 0}, {2, 0, 1});
}

// --- Replacing and deleting ----------------------------------------------------------

TEST(SelectEdit, TypingReplacesTheSelection) {
    auto file = Doc("abcd", {0, 0, 1});
    ShiftPress(file, KEY_RIGHT, 2);
    Type(file, "X");
    EXPECT_EQ(file.ContentText(), "aXd");
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 0, 2}));
    EXPECT_FALSE(file.GetSelection());
}

TEST(SelectEdit, BackspaceDeletesASelectedObjectAtOnce) {
    auto file = Doc("a\\frac{1}{2}b", {0, 2, 0});
    ShiftPress(file, KEY_LEFT);
    ASSERT_TRUE(Press(file, KEY_BACKSPACE));
    EXPECT_EQ(file.ContentText(), "ab");
    EXPECT_FALSE(file.GetPendingDelete());  // no red mark first
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 0, 1}));
}

TEST(SelectEdit, DeleteJoinsRowsAcrossTheSelection) {
    auto file = Doc("ab\n\\sqrt{x}\ncd", {0, 0, 1});
    file.PressKey(KEY_DOWN, true, nullptr);
    file.PressKey(KEY_DOWN, true, nullptr);  // row 2, start
    ShiftPress(file, KEY_RIGHT);
    ExpectSelection(file, {0, 0, 1}, {2, 0, 1});
    ASSERT_TRUE(Press(file, KEY_DELETE));
    EXPECT_EQ(file.ContentText(), "ad");
    EXPECT_EQ(file.content.size(), 1u);
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 0, 1}));
}

TEST(SelectEdit, SelectAllAndTypeReplacesEverything) {
    auto file = Doc("x\n\\frac{1}{2}\ny");
    file.SelectAll();
    Type(file, "z");
    EXPECT_EQ(file.ContentText(), "z");
    EXPECT_EQ(file.content.size(), 1u);
}

TEST(SelectEdit, EnterReplacesTheSelectionWithARowBreak) {
    auto file = Doc("abcd", {0, 0, 1});
    ShiftPress(file, KEY_RIGHT, 2);
    ASSERT_TRUE(Press(file, KEY_ENTER));
    EXPECT_EQ(file.ContentText(), "a\nd");
    EXPECT_EQ(file.cursorPos, (CursorPath{1, 0, 0}));
}

TEST(SelectEdit, InsertingAnObjectReplacesTheSelection) {
    auto file = Doc("abcd", {0, 0, 1});
    ShiftPress(file, KEY_RIGHT, 2);
    ASSERT_TRUE(file.InsertNode(
        std::make_unique<FracNode>(std::make_unique<LineNode>(), std::make_unique<LineNode>())));
    EXPECT_EQ(file.ContentText(), "a\\frac{}{}d");
}

TEST(SelectEdit, SelectionInsideASlotOnlyEditsThatSlot) {
    auto file = Doc("\\frac{123}{4}", {0, 1, 0, 0, 0});
    ShiftPress(file, KEY_RIGHT, 2);
    ASSERT_TRUE(Press(file, KEY_BACKSPACE));
    EXPECT_EQ(file.ContentText(), "\\frac{3}{4}");
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 1, 0, 0, 0}));
}

TEST(SelectEdit, BlockedCharactersKeepTheSelection) {
    auto file = Doc("abc", {0, 0, 0});
    ShiftPress(file, KEY_RIGHT);
    EXPECT_FALSE(file.TypeCharacter(U'{'));
    EXPECT_EQ(file.ContentText(), "abc");
    EXPECT_TRUE(file.GetSelection());
}
