// Cursor navigation through the M++ node tree. These run without a window or
// layout, so Up/Down land at the first position of their target.

#include "editor/FileMpp.h"
#include "raylib.h"
#include "test_helpers.h"

#include <gtest/gtest.h>
#include <string>

namespace {
FileMpp MakeDocument(const std::string& source) {
    FileMpp file;
    file.SetSource(source);
    return file;
}

bool PressKey(FileMpp& file,
              int key) {
    return file.PressKey(key, false, nullptr);
}

const EditorNode* NodeAt(const FileMpp& file,
                         std::size_t row,
                         std::size_t child) {
    return file.content[row]->GetChild(child);
}
}  // namespace

// --- Document structure ---------------------------------------------------------

TEST(CursorDocument,
     RowsArePaddedSoTextAlternatesWithOtherNodes) {
    auto file = MakeDocument("a\\frac{1}{2}\\alpha\\sqrt{x}b\n\n{c}");
    ASSERT_EQ(file.content.size(), 3u);
    EXPECT_EQ(test::Describe(file.content[0].get()),
              R"(line(text("a"), frac(line(text("1")), line(text("2"))), text(""), greek(alpha), )"
              R"(text(""), sqrt(line(text("x"))), text("b")))");
    EXPECT_EQ(test::Describe(file.content[1].get()), R"(line(text("")))");
    EXPECT_EQ(test::Describe(file.content[2].get()),
              R"(line(text(""), group(text("c")), text("")))");
}

TEST(CursorDocument,
     PaddingDoesNotChangeTheText) {
    for (const std::string source : {"",
                                     "plain",
                                     "a\\frac{1}{2}\\alpha",
                                     "\\sqrt[3]{\\frac{}{}}\n\nx\n",
                                     "\\begin{pmatrix}a&b\\\\\nc&d\\end{pmatrix}",
                                     "one\r\ntwo\r\n"}) {
        EXPECT_EQ(MakeDocument(source).ContentText(), source) << source;
    }
}

TEST(CursorDocument,
     CursorStartsAtTheBeginning) {
    auto file = MakeDocument("\\frac{1}{2}");
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 0, 0}));
    EXPECT_EQ(file.selectionStart, file.cursorPos);
}

// --- Left / Right -----------------------------------------------------------------

TEST(CursorMove,
     RightWalksThroughSqrtIndexThenArgument) {
    auto file = MakeDocument("\\sqrt[2]{56}");
    const std::vector<CursorPath> expected = {
        {0, 1, 0, 0, 0},  // index, before "2"
        {0, 1, 0, 0, 1},  // index, after "2"
        {0, 1, 1, 0, 0},  // argument, before "5"
        {0, 1, 1, 0, 1},
        {0, 1, 1, 0, 2},
        {0, 2, 0},  // after the root
    };
    for (const auto& position : expected) {
        ASSERT_TRUE(PressKey(file, KEY_RIGHT));
        EXPECT_EQ(file.cursorPos, position);
    }
    EXPECT_FALSE(PressKey(file, KEY_RIGHT));  // end of the document

    for (auto it = expected.rbegin() + 1; it != expected.rend(); ++it) {
        ASSERT_TRUE(PressKey(file, KEY_LEFT));
        EXPECT_EQ(file.cursorPos, *it);
    }
    ASSERT_TRUE(PressKey(file, KEY_LEFT));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 0, 0}));
    EXPECT_FALSE(PressKey(file, KEY_LEFT));
}

TEST(CursorMove,
     LeftAndRightLeaveAFractionFromEitherSlot) {
    auto file = MakeDocument("\\frac{1}{2}");
    file.MoveCursorTo({0, 1, 1, 0, 1});  // end of the denominator
    ASSERT_TRUE(PressKey(file, KEY_RIGHT));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 2, 0}));

    file.MoveCursorTo({0, 1, 1, 0, 0});  // start of the denominator
    ASSERT_TRUE(PressKey(file, KEY_LEFT));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 0, 0}));

    // Entering from the right lands at the end of the numerator.
    file.MoveCursorTo({0, 2, 0});
    ASSERT_TRUE(PressKey(file, KEY_LEFT));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 1, 0, 0, 1}));
}

TEST(CursorMove,
     StepsOverGreekLetters) {
    auto file = MakeDocument("a\\alpha b");
    file.MoveCursorTo({0, 0, 1});  // after "a", before alpha
    ASSERT_TRUE(PressKey(file, KEY_RIGHT));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 2, 0}));  // after alpha
    ASSERT_TRUE(PressKey(file, KEY_LEFT));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 0, 1}));
}

TEST(CursorMove,
     MovesByWholeUtf8Characters) {
    TextNode text("aé");  // 'é' is two bytes
    const std::size_t path[] = {1};
    KeyEvent event;
    event.key = KEY_RIGHT;
    event.path = path;
    const KeyResult result = text.KeyPress(event);
    EXPECT_EQ(result.kind, KeyResult::Kind::MOVED);
    EXPECT_EQ(result.path, (CursorPath{3}));
}

TEST(CursorMove,
     TextReportsExitsToItsParent) {
    TextNode text("ab");
    KeyEvent event;
    const std::size_t start[] = {0};
    const std::size_t end[] = {2};

    event.key = KEY_LEFT;
    event.path = start;
    EXPECT_EQ(text.KeyPress(event).kind, KeyResult::Kind::EXIT_LEFT);
    event.key = KEY_RIGHT;
    event.path = end;
    EXPECT_EQ(text.KeyPress(event).kind, KeyResult::Kind::EXIT_RIGHT);
    event.key = KEY_UP;
    EXPECT_EQ(text.KeyPress(event).kind, KeyResult::Kind::EXIT_UP);
    event.key = KEY_DOWN;
    EXPECT_EQ(text.KeyPress(event).kind, KeyResult::Kind::EXIT_DOWN);
}

TEST(CursorMove,
     LeftAndRightWrapBetweenRows) {
    auto file = MakeDocument("ab\ncd");
    file.MoveCursorTo({0, 0, 2});
    ASSERT_TRUE(PressKey(file, KEY_RIGHT));
    EXPECT_EQ(file.cursorPos, (CursorPath{1, 0, 0}));
    ASSERT_TRUE(PressKey(file, KEY_LEFT));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 0, 2}));
}

// --- Up / Down --------------------------------------------------------------------

TEST(CursorMove,
     UpAndDownMoveBetweenNumeratorAndDenominator) {
    auto file = MakeDocument("x\\frac{1}{2}\ny");
    file.MoveCursorTo({0, 1, 1, 0, 1});  // denominator
    ASSERT_TRUE(PressKey(file, KEY_UP));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 1, 0, 0, 0}));  // numerator
    ASSERT_TRUE(PressKey(file, KEY_DOWN));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 1, 1, 0, 0}));  // denominator

    // Down from the denominator leaves the fraction and goes to the next row.
    ASSERT_TRUE(PressKey(file, KEY_DOWN));
    EXPECT_EQ(file.cursorPos, (CursorPath{1, 0, 0}));
}

TEST(CursorMove,
     UpFromTheTopRowDoesNothing) {
    auto file = MakeDocument("\\frac{1}{2}");
    file.MoveCursorTo({0, 1, 0, 0, 1});  // numerator
    EXPECT_FALSE(PressKey(file, KEY_UP));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 1, 0, 0, 1}));
}

TEST(CursorMove,
     ArrowsMoveBetweenMatrixCells) {
    auto file = MakeDocument("\\begin{matrix}a&b\\\\c&d\\end{matrix}");
    ASSERT_TRUE(PressKey(file, KEY_RIGHT));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 1, 0, 0, 0, 0}));  // cell a
    ASSERT_TRUE(PressKey(file, KEY_RIGHT));
    ASSERT_TRUE(PressKey(file, KEY_RIGHT));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 1, 0, 1, 0, 0}));  // cell b
    ASSERT_TRUE(PressKey(file, KEY_DOWN));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 1, 1, 1, 0, 0}));  // cell d
    ASSERT_TRUE(PressKey(file, KEY_LEFT));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 1, 1, 0, 0, 1}));  // end of cell c
    ASSERT_TRUE(PressKey(file, KEY_UP));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 1, 0, 0, 0, 0}));  // cell a
    EXPECT_FALSE(PressKey(file, KEY_UP));  // leaves the matrix upwards, but row 0 is the top
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 1, 0, 0, 0, 0}));
}

TEST(CursorMove,
     ShiftKeepsTheSelectionStart) {
    auto file = MakeDocument("abc");
    ASSERT_TRUE(file.PressKey(KEY_RIGHT, true, nullptr));
    EXPECT_EQ(file.selectionStart, (CursorPath{0, 0, 0}));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 0, 1}));
    ASSERT_TRUE(file.PressKey(KEY_RIGHT, false, nullptr));
    EXPECT_EQ(file.selectionStart, file.cursorPos);
}

TEST(CursorMove,
     RecoversFromAnInvalidPath) {
    auto file = MakeDocument("ab");
    file.MoveCursorTo({5, 9});
    ASSERT_TRUE(PressKey(file, KEY_RIGHT));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 0, 1}));
}

// --- Green outline ------------------------------------------------------------------

TEST(CursorOutline,
     InnermostStructureHoldingTheCursorIsActive) {
    // \sqrt[2]{5|6}: the root's index and argument are outlined.
    auto file = MakeDocument("\\sqrt[2]{56}");
    file.MoveCursorTo({0, 1, 1, 0, 1});
    EXPECT_EQ(file.ActiveStructure(), NodeAt(file, 0, 1));

    // \sqrt[2]{\frac{1}{2|}}: only the fraction is outlined.
    file = MakeDocument("\\sqrt[2]{\\frac{1}{2}}");
    file.MoveCursorTo({0, 1, 1, 1, 1, 0, 1});
    ASSERT_NE(file.CursorText(file.cursorPos), nullptr);
    const auto* sqrt = dynamic_cast<const SqrtNode*>(NodeAt(file, 0, 1));
    ASSERT_NE(sqrt, nullptr);
    const auto* argument = dynamic_cast<const LineNode*>(sqrt->GetChildren().second);
    EXPECT_EQ(file.ActiveStructure(), argument->GetChild(1));
}

TEST(CursorOutline,
     NothingIsActiveOnAPlainRow) {
    auto file = MakeDocument("ab\\frac{1}{2}");
    file.MoveCursorTo({0, 0, 1});
    EXPECT_EQ(file.ActiveStructure(), nullptr);
}
