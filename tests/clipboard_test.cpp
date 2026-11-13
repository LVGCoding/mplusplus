#include "editor/FileMpp.h"
#include "editor/fileText.h"

#include <gtest/gtest.h>

TEST(ClipboardText, CopiesForwardAndReversedSelectionsWithoutChangingText) {
    FileText file;
    file.text = "aé日z\nnext";
    file.selectionStart = 1;
    file.cursorPos = 6;
    EXPECT_EQ(file.SelectedText(), "é日");
    std::swap(file.selectionStart, file.cursorPos);
    EXPECT_EQ(file.SelectedText(), "é日");
    EXPECT_FALSE(file.modified);
    file.selectionStart = file.cursorPos;
    EXPECT_TRUE(file.SelectedText().empty());
}

TEST(ClipboardText, PasteReplacesSelectionAndPreservesMultilineUtf8) {
    FileText file;
    file.text = "before old after";
    file.selectionStart = 10;
    file.cursorPos = 7;
    ASSERT_TRUE(file.PasteText("é日\nsecond"));
    EXPECT_EQ(file.text, "before é日\nsecond after");
    EXPECT_EQ(file.cursorPos, std::string("before é日\nsecond").size());
    EXPECT_EQ(file.selectionStart, file.cursorPos);
    EXPECT_TRUE(file.modified);
    ASSERT_TRUE(file.PasteText("!"));
    EXPECT_EQ(file.text, "before é日\nsecond! after");
}

TEST(ClipboardText, EmptyPasteKeepsSelectionAndContent) {
    FileText file;
    file.text = "keep";
    file.selectionStart = 0;
    file.cursorPos = 4;
    EXPECT_FALSE(file.PasteText(""));
    EXPECT_EQ(file.SelectedText(), "keep");
    EXPECT_FALSE(file.modified);
}

TEST(ClipboardText, PasteReplacesSelectionAsOneUndoStepAndClearsRedo) {
    FileText file;
    file.text = "before old after";
    file.selectionStart = 10;
    file.cursorPos = 7;
    ASSERT_TRUE(file.PasteText("é日\nsecond"));
    EXPECT_TRUE(file.CanUndo());
    EXPECT_FALSE(file.CanRedo());
    ASSERT_TRUE(file.Undo());
    EXPECT_EQ(file.text, "before old after");
    EXPECT_EQ(file.SelectedText(), "old");
    EXPECT_FALSE(file.CanUndo());
    EXPECT_TRUE(file.CanRedo());
    ASSERT_TRUE(file.Redo());
    EXPECT_EQ(file.text, "before é日\nsecond after");
    ASSERT_TRUE(file.Undo());
    ASSERT_TRUE(file.PasteText("new"));
    EXPECT_EQ(file.text, "before new after");
    EXPECT_FALSE(file.CanRedo());
}

TEST(ClipboardMpp, CopiesPartialTextAndWholeObjectsAcrossRows) {
    FileMpp file;
    file.SetSource("ab\\frac{x}{y}cd\nlast");
    file.selectionStart = {0, 0, 1};
    file.cursorPos = {1, 0, 2};
    EXPECT_EQ(file.SelectedText(), "b\\frac{x}{y}cd\nla");
    std::swap(file.selectionStart, file.cursorPos);
    EXPECT_EQ(file.SelectedText(), "b\\frac{x}{y}cd\nla");
    EXPECT_FALSE(file.CanUndo());
}

TEST(ClipboardMpp, CopiesJustTheSelectedTextInsideASlot) {
    FileMpp file;
    file.SetSource("\\frac{1234}{z}");
    file.selectionStart = {0, 1, 0, 0, 1};
    file.cursorPos = {0, 1, 0, 0, 3};
    EXPECT_EQ(file.SelectedText(), "23");
}

TEST(ClipboardMpp, SelectionAcrossMatrixCellsCopiesTheWholeMatrix) {
    FileMpp file;
    const std::string matrix = "\\begin{matrix}abcd&efgh\\end{matrix}";
    file.SetSource("before" + matrix + "after");
    file.selectionStart = {0, 1, 0, 0, 0, 1};
    file.cursorPos = {0, 1, 0, 1, 0, 2};
    EXPECT_EQ(file.SelectedText(), matrix);
}

TEST(ClipboardMpp, PasteParsesObjectsAndSplicesRowsAtCursor) {
    FileMpp file;
    file.SetSource("abcc");
    file.cursorPos = file.selectionStart = {0, 0, 2};
    ASSERT_TRUE(file.PasteText("ONE\r\n\\frac{x}{y}TWO"));
    EXPECT_EQ(file.ContentText(), "abONE\n\\frac{x}{y}TWOcc");
    EXPECT_EQ(file.source, file.ContentText());
    EXPECT_EQ(file.cursorPos, (CursorPath{1, 2, 3}));
    EXPECT_EQ(file.selectionStart, file.cursorPos);
    ASSERT_NE(file.CursorText(file.cursorPos), nullptr);
    EXPECT_TRUE(file.modified);
    ASSERT_TRUE(file.Undo());
    EXPECT_EQ(file.ContentText(), "abcc");
    EXPECT_FALSE(file.CanUndo());
    ASSERT_TRUE(file.Redo());
    EXPECT_EQ(file.ContentText(), "abONE\n\\frac{x}{y}TWOcc");
}

TEST(ClipboardMpp, PasteReplacesSelectionAsOneUndoStep) {
    FileMpp file;
    const std::string original = "a\\sqrt{x}b\nlast";
    file.SetSource(original);
    file.selectionStart = {0, 0, 1};
    file.cursorPos = {1, 0, 2};
    ASSERT_TRUE(file.PasteText("é\\sqrt{q}"));
    EXPECT_EQ(file.ContentText(), "aé\\sqrt{q}st");
    ASSERT_TRUE(file.Undo());
    EXPECT_EQ(file.ContentText(), original);
    EXPECT_EQ(file.SelectedText(), "\\sqrt{x}b\nla");
    EXPECT_FALSE(file.CanUndo());
}

TEST(ClipboardMpp, PasteInsideSlotPreservesNestedObjectsAndUsesSpacesForLineBreaks) {
    FileMpp file;
    file.SetSource("\\frac{pq}{z}");
    file.cursorPos = file.selectionStart = {0, 1, 0, 0, 1};
    ASSERT_TRUE(file.PasteText("x\n\\frac{a}{b}\ny"));
    EXPECT_EQ(file.ContentText(), "\\frac{px \\frac{a}{b} yq}{z}");
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 1, 0, 2, 2}));
    ASSERT_NE(file.CursorText(file.cursorPos), nullptr);
    EXPECT_FALSE(file.TakeMathMenuRequest());
    ASSERT_TRUE(file.Undo());
    EXPECT_EQ(file.ContentText(), "\\frac{pq}{z}");
}

TEST(ClipboardMpp, CopyPasteRoundTripsWholeDocumentIncludingEmptyRows) {
    FileMpp source;
    source.SetSource("\\sqrt[3]{x}\n\n\\begin{matrix}a&b\\\\c&d\\end{matrix}\n");
    source.SelectAll();
    FileMpp target;
    ASSERT_TRUE(target.PasteText(source.SelectedText()));
    EXPECT_EQ(target.ContentText(), source.ContentText());
    EXPECT_EQ(target.LineCount(), 4u);
    ASSERT_NE(target.CursorText(target.cursorPos), nullptr);
}

TEST(ClipboardMpp, EmptyPasteDoesNotDeleteSelectionOrCreateHistory) {
    FileMpp file;
    file.SetSource("keep");
    file.SelectAll();
    EXPECT_FALSE(file.PasteText(""));
    EXPECT_EQ(file.SelectedText(), "keep");
    EXPECT_FALSE(file.CanUndo());
}
