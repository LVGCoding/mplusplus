// Cursor navigation through the M++ node tree. These run without a window or
// layout, so Up/Down land at the first position of their target.

#include "editor/FileMpp.h"
#include "raylib.h"
#include "test_helpers.h"

#include <gtest/gtest.h>

#include <string>

namespace {
FileMpp Doc(const std::string& source) {
    FileMpp file;
    file.SetSource(source);
    return file;
}

bool Press(FileMpp& file, int key) {
    return file.PressKey(key, false, nullptr);
}

const EditorNode* NodeAt(const FileMpp& file, std::size_t row, std::size_t child) {
    return file.content[row]->GetChild(child);
}
}  // namespace

// --- Document structure ---------------------------------------------------------

TEST(CursorDocument, RowsArePaddedSoTextAlternatesWithOtherNodes) {
    auto file = Doc("a\\frac{1}{2}\\alpha\\sqrt{x}b\n\n{c}");
    ASSERT_EQ(file.content.size(), 3u);
    EXPECT_EQ(test::Describe(file.content[0].get()),
              R"(line(text("a"), frac(line(text("1")), line(text("2"))), text(""), greek(alpha), )"
              R"(text(""), sqrt(line(text("x"))), text("b")))");
    EXPECT_EQ(test::Describe(file.content[1].get()), R"(line(text("")))");
    EXPECT_EQ(test::Describe(file.content[2].get()), R"(line(text(""), group(text("c")), text("")))");
}

TEST(CursorDocument, PaddingDoesNotChangeTheText) {
    for (const std::string source :
         {"", "plain", "a\\frac{1}{2}\\alpha", "\\sqrt[3]{\\frac{}{}}\n\nx\n",
          "\\begin{pmatrix}a&b\\\\\nc&d\\end{pmatrix}", "one\r\ntwo\r\n"}) {
        EXPECT_EQ(Doc(source).ContentText(), source) << source;
    }
}

TEST(CursorDocument, CursorStartsAtTheBeginning) {
    auto file = Doc("\\frac{1}{2}");
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 0, 0}));
    EXPECT_EQ(file.selectionStart, file.cursorPos);
}

// --- Left / Right -----------------------------------------------------------------

TEST(CursorMove, RightWalksThroughSqrtIndexThenArgument) {
    auto file = Doc("\\sqrt[2]{56}");
    const std::vector<CursorPath> expected = {
        {0, 1, 0, 0, 0},  // index, before "2"
        {0, 1, 0, 0, 1},  // index, after "2"
        {0, 1, 1, 0, 0},  // argument, before "5"
        {0, 1, 1, 0, 1},
        {0, 1, 1, 0, 2},
        {0, 2, 0},  // after the root
    };
    for (const auto& position : expected) {
        ASSERT_TRUE(Press(file, KEY_RIGHT));
        EXPECT_EQ(file.cursorPos, position);
    }
    EXPECT_FALSE(Press(file, KEY_RIGHT));  // end of the document

    for (auto it = expected.rbegin() + 1; it != expected.rend(); ++it) {
        ASSERT_TRUE(Press(file, KEY_LEFT));
        EXPECT_EQ(file.cursorPos, *it);
    }
    ASSERT_TRUE(Press(file, KEY_LEFT));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 0, 0}));
    EXPECT_FALSE(Press(file, KEY_LEFT));
}

TEST(CursorMove, LeftAndRightLeaveAFractionFromEitherSlot) {
    auto file = Doc("\\frac{1}{2}");
    file.cursorPos = {0, 1, 1, 0, 1};  // end of the denominator
    ASSERT_TRUE(Press(file, KEY_RIGHT));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 2, 0}));

    file.cursorPos = {0, 1, 1, 0, 0};  // start of the denominator
    ASSERT_TRUE(Press(file, KEY_LEFT));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 0, 0}));

    // Entering from the right lands at the end of the numerator.
    file.cursorPos = {0, 2, 0};
    ASSERT_TRUE(Press(file, KEY_LEFT));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 1, 0, 0, 1}));
}

TEST(CursorMove, StepsOverGreekLetters) {
    auto file = Doc("a\\alpha b");
    file.cursorPos = {0, 0, 1};  // after "a", before alpha
    ASSERT_TRUE(Press(file, KEY_RIGHT));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 2, 0}));  // after alpha
    ASSERT_TRUE(Press(file, KEY_LEFT));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 0, 1}));
}

TEST(CursorMove, MovesByWholeUtf8Characters) {
    TextNode text("aé");  // 'é' is two bytes
    const std::size_t path[] = {1};
    KeyEvent event;
    event.key = KEY_RIGHT;
    event.path = path;
    const KeyResult result = text.KeyPress(event);
    EXPECT_EQ(result.kind, KeyResult::Kind::Moved);
    EXPECT_EQ(result.path, (CursorPath{3}));
}

TEST(CursorMove, TextReportsExitsToItsParent) {
    TextNode text("ab");
    KeyEvent event;
    const std::size_t start[] = {0};
    const std::size_t end[] = {2};

    event.key = KEY_LEFT;
    event.path = start;
    EXPECT_EQ(text.KeyPress(event).kind, KeyResult::Kind::ExitLeft);
    event.key = KEY_RIGHT;
    event.path = end;
    EXPECT_EQ(text.KeyPress(event).kind, KeyResult::Kind::ExitRight);
    event.key = KEY_UP;
    EXPECT_EQ(text.KeyPress(event).kind, KeyResult::Kind::ExitUp);
    event.key = KEY_DOWN;
    EXPECT_EQ(text.KeyPress(event).kind, KeyResult::Kind::ExitDown);
}

TEST(CursorMove, LeftAndRightWrapBetweenRows) {
    auto file = Doc("ab\ncd");
    file.cursorPos = {0, 0, 2};
    ASSERT_TRUE(Press(file, KEY_RIGHT));
    EXPECT_EQ(file.cursorPos, (CursorPath{1, 0, 0}));
    ASSERT_TRUE(Press(file, KEY_LEFT));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 0, 2}));
}

// --- Up / Down --------------------------------------------------------------------

TEST(CursorMove, UpAndDownMoveBetweenNumeratorAndDenominator) {
    auto file = Doc("x\\frac{1}{2}\ny");
    file.cursorPos = {0, 1, 1, 0, 1};  // denominator
    ASSERT_TRUE(Press(file, KEY_UP));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 1, 0, 0, 0}));  // numerator
    ASSERT_TRUE(Press(file, KEY_DOWN));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 1, 1, 0, 0}));  // denominator

    // Down from the denominator leaves the fraction and goes to the next row.
    ASSERT_TRUE(Press(file, KEY_DOWN));
    EXPECT_EQ(file.cursorPos, (CursorPath{1, 0, 0}));
}

TEST(CursorMove, UpFromTheTopRowDoesNothing) {
    auto file = Doc("\\frac{1}{2}");
    file.cursorPos = {0, 1, 0, 0, 1};  // numerator
    EXPECT_FALSE(Press(file, KEY_UP));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 1, 0, 0, 1}));
}

TEST(CursorMove, ArrowsMoveBetweenMatrixCells) {
    auto file = Doc("\\begin{matrix}a&b\\\\c&d\\end{matrix}");
    ASSERT_TRUE(Press(file, KEY_RIGHT));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 1, 0, 0, 0, 0}));  // cell a
    ASSERT_TRUE(Press(file, KEY_RIGHT));
    ASSERT_TRUE(Press(file, KEY_RIGHT));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 1, 0, 1, 0, 0}));  // cell b
    ASSERT_TRUE(Press(file, KEY_DOWN));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 1, 1, 1, 0, 0}));  // cell d
    ASSERT_TRUE(Press(file, KEY_LEFT));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 1, 1, 0, 0, 1}));  // end of cell c
    ASSERT_TRUE(Press(file, KEY_UP));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 1, 0, 0, 0, 0}));  // cell a
    EXPECT_FALSE(Press(file, KEY_UP));  // leaves the matrix upwards, but row 0 is the top
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 1, 0, 0, 0, 0}));
}

TEST(CursorMove, ShiftKeepsTheSelectionStart) {
    auto file = Doc("abc");
    ASSERT_TRUE(file.PressKey(KEY_RIGHT, true, nullptr));
    EXPECT_EQ(file.selectionStart, (CursorPath{0, 0, 0}));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 0, 1}));
    ASSERT_TRUE(file.PressKey(KEY_RIGHT, false, nullptr));
    EXPECT_EQ(file.selectionStart, file.cursorPos);
}

TEST(CursorMove, RecoversFromAnInvalidPath) {
    auto file = Doc("ab");
    file.cursorPos = {5, 9};
    ASSERT_TRUE(Press(file, KEY_RIGHT));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 0, 1}));
}

// --- Green outline ------------------------------------------------------------------

TEST(CursorOutline, InnermostStructureHoldingTheCursorIsActive) {
    // \sqrt[2]{5|6}: the root's index and argument are outlined.
    auto file = Doc("\\sqrt[2]{56}");
    file.cursorPos = {0, 1, 1, 0, 1};
    EXPECT_EQ(file.ActiveStructure(), NodeAt(file, 0, 1));

    // \sqrt[2]{\frac{1}{2|}}: only the fraction is outlined.
    file = Doc("\\sqrt[2]{\\frac{1}{2}}");
    file.cursorPos = {0, 1, 1, 1, 1, 0, 1};
    ASSERT_NE(file.CursorText(file.cursorPos), nullptr);
    const auto* sqrt = dynamic_cast<const SqrtNode*>(NodeAt(file, 0, 1));
    ASSERT_NE(sqrt, nullptr);
    const auto* argument = dynamic_cast<const LineNode*>(sqrt->GetChildren().second);
    EXPECT_EQ(file.ActiveStructure(), argument->GetChild(1));
}

TEST(CursorOutline, NothingIsActiveOnAPlainRow) {
    auto file = Doc("ab\\frac{1}{2}");
    file.cursorPos = {0, 0, 1};
    EXPECT_EQ(file.ActiveStructure(), nullptr);
}
