// Double and triple clicks (editor/ClickSelect.h, FileMpp word/row selection).

#include "editor/ClickSelect.h"
#include "editor/FileMpp.h"

#include <gtest/gtest.h>

#include <string>
#include <utility>

namespace {
using Range = std::pair<std::size_t, std::size_t>;

FileMpp Document(const std::string& source, CursorPath cursor) {
    FileMpp file;
    file.SetSource(source);
    file.cursorPos = cursor;
    file.selectionStart = cursor;
    return file;
}
}  // namespace

TEST(ClickCounter, CountsQuickClicksInOnePlace) {
    ClickCounter clicks;
    EXPECT_EQ(clicks.Press(1.0, 10, 10), 1);
    EXPECT_EQ(clicks.Press(1.2, 11, 10), 2);
    EXPECT_EQ(clicks.Press(1.4, 11, 11), 3);
    EXPECT_EQ(clicks.Press(1.5, 11, 11), 1);  // a fourth starts again
    // Too slow, or too far away: a new single click.
    EXPECT_EQ(clicks.Press(3.0, 11, 11), 1);
    EXPECT_EQ(clicks.Press(3.1, 40, 11), 1);
}

TEST(WordAt, FindsTheWordSpacesOrSymbolsUnderTheClick) {
    const std::string text = "print(total + 12)";
    EXPECT_EQ(WordAt(text, 2), (Range{0, 5}));     // in "print"
    EXPECT_EQ(WordAt(text, 5), (Range{0, 5}));     // just after it: still the word
    EXPECT_EQ(WordAt(text, 8), (Range{6, 11}));    // "total"
    EXPECT_EQ(WordAt(text, 15), (Range{14, 16}));  // "12"
    EXPECT_EQ(WordAt(text, 12), (Range{12, 13}));  // "+"
    EXPECT_EQ(WordAt("a   b", 2), (Range{1, 4}));  // the spaces
    EXPECT_EQ(WordAt("v_max+1", 1), (Range{0, 5}));
    EXPECT_EQ(WordAt("ab", 2), (Range{0, 2}));     // at the end
    EXPECT_EQ(WordAt("", 0), (Range{0, 0}));
    // Lines: a click past the end of a line takes its last word.
    EXPECT_EQ(WordAt("ab\ncd", 2), (Range{0, 2}));
    EXPECT_EQ(WordAt("x+\n-y", 2), (Range{1, 2}));
}

TEST(ClickSelect, DoubleClickSelectsAWordEvenInsideAFraction) {
    auto file = Document("total := count + 1", {0, 0, 11});
    ASSERT_TRUE(file.SelectWordAtCursor());
    EXPECT_EQ(file.SelectedText(), "count");
    // In the numerator of a fraction.
    file = Document("y := \\frac{width}{2}", {0, 1, 0, 0, 2});
    ASSERT_TRUE(file.SelectWordAtCursor());
    EXPECT_EQ(file.SelectedText(), "width");
}

TEST(ClickSelect, TripleClickSelectsTheRow) {
    auto file = Document("a := 1\nb := \\frac{a}{2}\nc", {1, 1, 0, 0, 0});
    file.SelectRow(1);
    EXPECT_EQ(file.SelectedText(), "b := \\frac{a}{2}");
}
