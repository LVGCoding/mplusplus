// Typing, deleting and inserting in M++ documents. Runs without a window.

#include "editor/FileMpp.h"
#include "raylib.h"
#include "test_helpers.h"

#include <gtest/gtest.h>
#include <string>

namespace {
using Part = DeleteHighlight::Part;

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
              int key) {
    return file.PressKey(key, false, nullptr);
}

// These fixtures type ASCII bytes; UTF-8 cases exercise cursor movement or paste separately.
void TypeAscii(FileMpp& file,
               const std::string& text) {
    for (const char character : text) {
        file.TypeCharacter(static_cast<char32_t>(character));
    }
}

constexpr const char* MATRIX_2X2 = "\\begin{matrix}a&b\\\\c&d\\end{matrix}";
}  // namespace

// --- Text ---------------------------------------------------------------------------

TEST(EditText,
     TypingInsertsAtTheCursorAndKeepsSourceInSync) {
    auto file = MakeDocument("ac", {0, 0, 1});
    TypeAscii(file, "b");
    EXPECT_EQ(file.ContentText(), "abc");
    EXPECT_EQ(file.source, "abc");
    EXPECT_TRUE(file.modified);
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 0, 2}));
}

TEST(EditText,
     TypingInsideAFraction) {
    auto file = MakeDocument("\\frac{}{}", {0, 1, 1, 0, 0});
    TypeAscii(file, "x+1");
    EXPECT_EQ(file.ContentText(), "\\frac{}{x+1}");
}

TEST(EditText,
     BackspaceAndDeleteRemoveOneCharacter) {
    auto file = MakeDocument("abc", {0, 0, 2});
    ASSERT_TRUE(PressKey(file, KEY_BACKSPACE));
    EXPECT_EQ(file.ContentText(), "ac");
    ASSERT_TRUE(PressKey(file, KEY_DELETE));
    EXPECT_EQ(file.ContentText(), "a");
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 0, 1}));
}

TEST(EditText,
     BracesAreNotTyped) {
    auto file = MakeDocument("a", {0, 0, 1});
    EXPECT_FALSE(file.TypeCharacter(U'{'));
    EXPECT_FALSE(file.TypeCharacter(U'}'));
    EXPECT_EQ(file.ContentText(), "a");
}

TEST(EditText,
     BackslashAsksForTheMathPopup) {
    auto file = MakeDocument("a", {0, 0, 1});
    EXPECT_TRUE(file.TypeCharacter(U'\\'));
    EXPECT_EQ(file.ContentText(), "a");
    EXPECT_TRUE(file.TakeMathMenuRequest());
    EXPECT_FALSE(file.TakeMathMenuRequest());
}

// --- Rows -----------------------------------------------------------------------------

TEST(EditRows,
     EnterSplitsARowAndBackspaceJoinsItBack) {
    auto file = MakeDocument("ab\\alpha+cd", {0, 2, 1});
    ASSERT_TRUE(PressKey(file, KEY_ENTER));
    EXPECT_EQ(file.ContentText(), "ab\\alpha+\ncd");
    EXPECT_EQ(file.cursorPos, (CursorPath{1, 0, 0}));
    ASSERT_TRUE(PressKey(file, KEY_BACKSPACE));
    EXPECT_EQ(file.ContentText(), "ab\\alpha+cd");
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 2, 1}));
}

TEST(EditRows,
     DeleteAtTheEndOfARowPullsTheNextRowUp) {
    auto file = MakeDocument("ab\ncd", {0, 0, 2});
    ASSERT_TRUE(PressKey(file, KEY_DELETE));
    EXPECT_EQ(file.ContentText(), "abcd");
    EXPECT_EQ(file.LineCount(), 1u);
}

TEST(EditRows,
     EnterInsideAFractionDoesNothing) {
    auto file = MakeDocument("\\frac{1}{2}", {0, 1, 0, 0, 1});
    EXPECT_FALSE(PressKey(file, KEY_ENTER));
    EXPECT_EQ(file.LineCount(), 1u);
}

// --- Deleting objects ---------------------------------------------------------------------

TEST(EditDelete,
     GreekLettersAreDeletedLikeCharacters) {
    auto file = MakeDocument("a\\alpha+b", {0, 2, 0});
    ASSERT_TRUE(PressKey(file, KEY_BACKSPACE));
    EXPECT_EQ(file.ContentText(), "a+b");
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 0, 1}));
}

TEST(EditDelete,
     FirstBackspaceMarksAFractionSecondDeletesIt) {
    auto file = MakeDocument("x\\frac{1}{2}y", {0, 2, 0});
    ASSERT_TRUE(PressKey(file, KEY_BACKSPACE));
    ASSERT_TRUE(file.GetPendingDelete());
    EXPECT_EQ(file.GetPendingDelete()->node, (CursorPath{0, 1}));
    EXPECT_EQ(file.GetPendingDelete()->part, Part::WHOLE);
    EXPECT_EQ(file.ContentText(), "x\\frac{1}{2}y");

    ASSERT_TRUE(PressKey(file, KEY_BACKSPACE));
    EXPECT_FALSE(file.GetPendingDelete());
    EXPECT_EQ(file.ContentText(), "xy");
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 0, 1}));
}

TEST(EditDelete,
     AnyOtherKeyCancelsTheMark) {
    auto file = MakeDocument("x\\frac{1}{2}", {0, 2, 0});
    ASSERT_TRUE(PressKey(file, KEY_BACKSPACE));
    ASSERT_TRUE(PressKey(file, KEY_LEFT));  // cancels and moves
    EXPECT_FALSE(file.GetPendingDelete());
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 1, 0, 0, 1}));

    ASSERT_TRUE(PressKey(file, KEY_RIGHT));
    ASSERT_TRUE(PressKey(file, KEY_BACKSPACE));
    ASSERT_TRUE(PressKey(file, KEY_ESCAPE));
    EXPECT_FALSE(file.GetPendingDelete());
    EXPECT_EQ(file.ContentText(), "x\\frac{1}{2}");
}

TEST(EditDelete,
     DeleteImmediatelySettingSkipsTheMark) {
    auto file = MakeDocument("x\\sqrt{2}", {0, 2, 0});
    file.backspaceMode = editor_settings::StructureBackspace::DELETE_IMMEDIATELY;
    ASSERT_TRUE(PressKey(file, KEY_BACKSPACE));
    EXPECT_FALSE(file.GetPendingDelete());
    EXPECT_EQ(file.ContentText(), "x");
}

TEST(EditDelete,
     DeleteKeyMarksTheObjectAfterTheCursor) {
    auto file = MakeDocument("x\\frac{1}{2}", {0, 0, 1});
    ASSERT_TRUE(PressKey(file, KEY_DELETE));
    ASSERT_TRUE(file.GetPendingDelete());
    ASSERT_TRUE(PressKey(file, KEY_DELETE));
    EXPECT_EQ(file.ContentText(), "x");
}

TEST(EditDelete,
     BackspaceAtTheStartOfASlot) {
    // Start of the denominator: back to the end of the numerator.
    auto file = MakeDocument("\\frac{1}{2}", {0, 1, 1, 0, 0});
    ASSERT_TRUE(PressKey(file, KEY_BACKSPACE));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 1, 0, 0, 1}));
    EXPECT_FALSE(file.GetPendingDelete());

    // Start of the numerator: marks the fraction.
    file.MoveCursorTo({0, 1, 0, 0, 0});
    ASSERT_TRUE(PressKey(file, KEY_BACKSPACE));
    ASSERT_TRUE(file.GetPendingDelete());
    EXPECT_EQ(file.GetPendingDelete()->node, (CursorPath{0, 1}));

    // Start of a root's argument after an index: into the index.
    file = MakeDocument("\\sqrt[3]{x}", {0, 1, 1, 0, 0});
    ASSERT_TRUE(PressKey(file, KEY_BACKSPACE));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 1, 0, 0, 1}));
}

TEST(EditDelete,
     NestedObjectsAreDeletedFromTheirOwnSlot) {
    auto file = MakeDocument("\\sqrt{a\\frac{1}{2}}", {0, 1, 1, 2, 0});
    file.backspaceMode = editor_settings::StructureBackspace::DELETE_IMMEDIATELY;
    ASSERT_TRUE(PressKey(file, KEY_BACKSPACE));
    EXPECT_EQ(file.ContentText(), "\\sqrt{a}");
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 1, 1, 0, 1}));
}

// --- Matrix rows and columns ----------------------------------------------------------------

TEST(EditMatrix,
     BackspaceAtCellStartMarksTheRowThenDeletesIt) {
    auto file = MakeDocument(MATRIX_2X2, {0, 1, 1, 0, 0, 0});  // start of cell c
    ASSERT_TRUE(PressKey(file, KEY_BACKSPACE));
    ASSERT_TRUE(file.GetPendingDelete());
    EXPECT_EQ(file.GetPendingDelete()->node, (CursorPath{0, 1}));
    EXPECT_EQ(file.GetPendingDelete()->part, Part::ROW);
    EXPECT_EQ(file.GetPendingDelete()->row, 1u);

    ASSERT_TRUE(PressKey(file, KEY_BACKSPACE));
    EXPECT_EQ(file.ContentText(), "\\begin{matrix}a&b\\end{matrix}");
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 1, 0, 0, 0, 0}));
}

TEST(EditMatrix,
     TabSwitchesToTheColumn) {
    auto file = MakeDocument(MATRIX_2X2, {0, 1, 1, 0, 0, 0});
    ASSERT_TRUE(PressKey(file, KEY_BACKSPACE));
    ASSERT_TRUE(PressKey(file, KEY_TAB));
    EXPECT_EQ(file.GetPendingDelete()->part, Part::COLUMN);
    ASSERT_TRUE(PressKey(file, KEY_TAB));
    EXPECT_EQ(file.GetPendingDelete()->part, Part::ROW);
    ASSERT_TRUE(PressKey(file, KEY_TAB));
    ASSERT_TRUE(PressKey(file, KEY_BACKSPACE));
    EXPECT_EQ(file.ContentText(), "\\begin{matrix}b\\\\d\\end{matrix}");
}

TEST(EditMatrix,
     LastRowAndColumnAreKept) {
    auto file = MakeDocument("\\begin{matrix}a&b\\end{matrix}", {0, 1, 0, 1, 0, 0});
    ASSERT_TRUE(PressKey(file, KEY_BACKSPACE));
    ASSERT_TRUE(PressKey(file, KEY_BACKSPACE));  // only row: not deleted
    EXPECT_FALSE(file.GetPendingDelete());
    EXPECT_EQ(file.ContentText(), "\\begin{matrix}a&b\\end{matrix}");

    file = MakeDocument("\\begin{matrix}a\\\\b\\end{matrix}", {0, 1, 1, 0, 0, 0});
    ASSERT_TRUE(PressKey(file, KEY_BACKSPACE));
    ASSERT_TRUE(PressKey(file, KEY_TAB));
    ASSERT_TRUE(PressKey(file, KEY_BACKSPACE));  // only column: not deleted
    EXPECT_EQ(file.ContentText(), "\\begin{matrix}a\\\\b\\end{matrix}");
}

TEST(EditMatrix,
     BackspaceInsideACellStillDeletesText) {
    auto file = MakeDocument(MATRIX_2X2, {0, 1, 0, 0, 0, 1});
    ASSERT_TRUE(PressKey(file, KEY_BACKSPACE));
    EXPECT_FALSE(file.GetPendingDelete());
    EXPECT_EQ(file.ContentText(), "\\begin{matrix}&b\\\\c&d\\end{matrix}");
}

TEST(EditMatrix,
     AmpersandIsNotTypedInACell) {
    auto file = MakeDocument(MATRIX_2X2, {0, 1, 0, 0, 0, 1});
    EXPECT_FALSE(file.TypeCharacter(U'&'));
    EXPECT_EQ(file.ContentText(), MATRIX_2X2);
}

TEST(EditMatrix,
     WheelInsertsInTheChosenDirection) {
    struct Case {
        int key;
        const char* expected;
        CursorPath cursor;
    };
    const Case cases[] = {
        {KEY_RIGHT, "\\begin{matrix}a&&b\\\\c&&d\\end{matrix}", {0, 1, 0, 1, 0, 0}},
        {KEY_LEFT, "\\begin{matrix}&a&b\\\\&c&d\\end{matrix}", {0, 1, 0, 0, 0, 0}},
        {KEY_UP, "\\begin{matrix}&\\\\a&b\\\\c&d\\end{matrix}", {0, 1, 0, 0, 0, 0}},
        {KEY_DOWN, "\\begin{matrix}a&b\\\\&\\\\c&d\\end{matrix}", {0, 1, 1, 0, 0, 0}},
    };
    for (const Case& testCase : cases) {
        auto file = MakeDocument(MATRIX_2X2, {0, 1, 0, 0, 0, 1});  // end of cell a
        ASSERT_TRUE(PressKey(file, KEY_ENTER));
        ASSERT_TRUE(file.GetInsertWheel());
        ASSERT_TRUE(PressKey(file, testCase.key));
        EXPECT_EQ(file.ContentText(), MATRIX_2X2) << "nothing inserted before Enter";
        ASSERT_TRUE(PressKey(file, KEY_ENTER));
        EXPECT_FALSE(file.GetInsertWheel());
        EXPECT_EQ(file.ContentText(), testCase.expected);
        EXPECT_EQ(file.cursorPos, testCase.cursor);
    }
}

TEST(EditMatrix,
     WheelCanBeCancelled) {
    auto file = MakeDocument(MATRIX_2X2, {0, 1, 0, 0, 0, 1});
    ASSERT_TRUE(PressKey(file, KEY_ENTER));
    EXPECT_FALSE(file.TypeCharacter(U'x'));  // typing is paused while it is open
    ASSERT_TRUE(PressKey(file, KEY_ESCAPE));
    EXPECT_FALSE(file.GetInsertWheel());

    ASSERT_TRUE(PressKey(file, KEY_ENTER));
    ASSERT_TRUE(PressKey(file, KEY_ENTER));  // Enter with no direction just closes
    EXPECT_FALSE(file.GetInsertWheel());
    EXPECT_EQ(file.ContentText(), MATRIX_2X2);
}

// --- Inserting objects -----------------------------------------------------------------------

TEST(EditInsert,
     InsertsAtTheCursorAndMovesIntoTheObject) {
    auto file = MakeDocument("ab", {0, 0, 1});
    ASSERT_TRUE(file.InsertNode(
        std::make_unique<FracNode>(std::make_unique<LineNode>(), std::make_unique<LineNode>())));
    EXPECT_EQ(file.ContentText(), "a\\frac{}{}b");
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 1, 0, 0, 0}));
    TypeAscii(file, "1");
    EXPECT_EQ(file.ContentText(), "a\\frac{1}{}b");
}

TEST(EditInsert,
     SymbolsAreSteppedOver) {
    auto file = MakeDocument("ab", {0, 0, 1});
    ASSERT_TRUE(file.InsertNode(std::make_unique<GreekNode>(GreekNode::GREEK_PI)));
    // Not "a\\pib": that would read back as one unknown command.
    EXPECT_EQ(file.ContentText(), "a\\pi b");
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 2, 0}));
}

TEST(EditInsert,
     RootIndexRejectsClosingBracket) {
    auto file = MakeDocument("\\sqrt[3]{x}", {0, 1, 0, 0, 1});
    EXPECT_FALSE(file.TypeCharacter(U']'));
    EXPECT_EQ(file.ContentText(), "\\sqrt[3]{x}");
}
