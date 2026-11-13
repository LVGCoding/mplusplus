#include "editor/FileMpp.h"
#include "editor/fileText.h"
#include "math/TextNode.h"
#include "parser/parser.h"
#include "ui/layout_panels.h"

#include <cmath>
#include <gtest/gtest.h>
#include <vector>

namespace {
class LineLayoutTest : public ::testing::Test {
protected:
    std::vector<char> memory_;
    std::vector<std::string> errors_;
    Clay_Context* previousContext_ = nullptr;

    void SetUp() override {
        previousContext_ = Clay_GetCurrentContext();
        memory_.resize(Clay_MinMemorySize());
        Clay_Initialize(Clay_CreateArenaWithCapacityAndMemory(memory_.size(), memory_.data()),
                        {800, 600},
                        {[](Clay_ErrorData error) {
                             auto* errors = static_cast<std::vector<std::string>*>(error.userData);
                             errors->emplace_back(error.errorText.chars, error.errorText.length);
                         },
                         &errors_});
        Clay_SetMeasureTextFunction(
            [](Clay_StringSlice text, Clay_TextElementConfig* config, void*) -> Clay_Dimensions {
                return {text.length * config->fontSize * 0.5f, float(config->fontSize)};
            },
            nullptr);
    }

    void TearDown() override {
        Clay_SetCurrentContext(previousContext_);
    }

    void LayoutFile(const FileBase& file) {
        Clay_BeginLayout();
        layout_internal::LayoutWorkspace(&file);
        Clay_EndLayout(0);
        EXPECT_TRUE(errors_.empty());
    }

    Clay_BoundingBox Row(const FileMpp& file,
                         std::size_t line) {
        const auto data = Clay_GetElementData(
            Clay_GetElementIdWithIndex(CLAY_STRING("MppRow"), static_cast<uint32_t>(line)));
        EXPECT_TRUE(data.found);
        EXPECT_NE(file.LineElementId(line), 0u);
        return data.boundingBox;
    }

    Clay_BoundingBox Label(std::size_t line) {
        const auto data = Clay_GetElementData(
            Clay_GetElementIdWithIndex(CLAY_STRING("GutterLine"), static_cast<uint32_t>(line)));
        EXPECT_TRUE(data.found);
        return data.boundingBox;
    }
};
}  // namespace

TEST_F(LineLayoutTest,
       NumbersFollowTallRowsOnFirstFrameAndAfterResize) {
    FileMpp file;
    file.SetSource("plain\n\\frac{a}{b}\n\\sqrt[3]{x}\n"
                                      "\\begin{matrix}a&b\\\\c&d\\end{matrix}\n\\alpha");
    ASSERT_EQ(file.LineCount(), 5u);
    LayoutFile(file);
    for (std::size_t line = 0; line < file.LineCount(); ++line) {
        EXPECT_FLOAT_EQ(Label(line).y, Row(file, line).y);
        EXPECT_FLOAT_EQ(Label(line).x, 10);
        if (line > 0) {
            const auto previous = Row(file, line - 1);
            EXPECT_FLOAT_EQ(Label(line).y, previous.y + previous.height);
        }
    }
    EXPECT_GT(Row(file, 1).height, editor_style::lineHeight);
    EXPECT_GT(Row(file, 3).height, editor_style::lineHeight);
    EXPECT_FLOAT_EQ(Label(5).y, Row(file, 4).y + Row(file, 4).height);

    file.SetSource("\\frac{\\frac{a}{b}}{c}\nnext");
    LayoutFile(file);
    EXPECT_FLOAT_EQ(Label(1).y, Row(file, 0).y + Row(file, 0).height);
    file.SetSource("short\nnext");
    LayoutFile(file);
    EXPECT_FLOAT_EQ(Row(file, 0).height, editor_style::lineHeight);
    EXPECT_FLOAT_EQ(Label(1).y - Label(0).y, editor_style::lineHeight);
}

TEST_F(LineLayoutTest,
       EmptyRowsAndNestedNewlinesUseRenderedLineCount) {
    FileMpp file;
    file.SetSource("\n\\begin{matrix}a&b\\\\\nc&d\\end{matrix}\n\n");
    ASSERT_EQ(file.LineCount(), 4u);
    LayoutFile(file);
    for (std::size_t line = 0; line < file.LineCount(); ++line) {
        EXPECT_FLOAT_EQ(Label(line).y, Row(file, line).y);
    }
    FileMpp empty;
    EXPECT_EQ(empty.LineCount(), 1u);
    LayoutFile(empty);
    EXPECT_FLOAT_EQ(Label(1).y - Label(0).y, editor_style::lineHeight);
}

TEST_F(LineLayoutTest,
       PlainTextKeepsFixedHeightGutter) {
    FileText file;
    file.text = "first\n\nthird";
    EXPECT_EQ(file.LineElementId(0), 0u);
    LayoutFile(file);
    EXPECT_TRUE(Clay_GetElementData(CLAY_ID("Gutter")).found);
}

TEST_F(LineLayoutTest,
       NumbersScrollWithTheirRows) {
    FileMpp file;
    std::string source;
    for (int line = 0; line < 30; ++line) {
        source += "\\frac{a}{b}\n";
    }
    file.SetSource(source);
    LayoutFile(file);
    auto scroll = Clay_GetScrollContainerData(CLAY_ID("Workspace"));
    EXPECT_TRUE(scroll.found);
    const float previousY = Row(file, 10).y;
    scroll.scrollPosition->y = -100;
    LayoutFile(file);
    EXPECT_FLOAT_EQ(Row(file, 10).y, previousY - 100);
    EXPECT_FLOAT_EQ(Label(10).y, Row(file, 10).y);
}

TEST_F(LineLayoutTest, ClickChoosesRenderedRowAndClampsToTextEnds) {
    FileMpp file;
    file.SetSource("wide text\n\\frac{a}{b}\n\nlast");
    LayoutFile(file);
    const TextMeasure measure = [](std::string_view text, float size) {
        return text.size() * size * 0.5f;
    };
    const auto last = Row(file, 3);
    ASSERT_TRUE(file.ClickAt(last.x + 1000, last.y + last.height / 2, &measure));
    EXPECT_EQ(file.cursorPos, (CursorPath{3, 0, 4}));
    ASSERT_TRUE(file.ClickAt(last.x - 100, last.y + last.height / 2, &measure));
    EXPECT_EQ(file.cursorPos, (CursorPath{3, 0, 0}));
    const auto empty = Row(file, 2);
    ASSERT_TRUE(file.ClickAt(empty.x + 100, empty.y + empty.height / 2, &measure));
    EXPECT_EQ(file.cursorPos, (CursorPath{2, 0, 0}));
    ASSERT_TRUE(file.ClickAt(last.x, last.y, &measure));
    EXPECT_EQ(file.cursorPos, (CursorPath{3, 0, 0}));
    ASSERT_TRUE(file.ClickAt(last.x + 1000, last.y + 1000, &measure));
    EXPECT_EQ(file.cursorPos, (CursorPath{3, 0, 4}));
    EXPECT_FALSE(file.modified);
}

TEST_F(LineLayoutTest, ClickEntersNestedSlotsAndUsesTheirFontSizes) {
    FileMpp file;
    file.SetSource("\\frac{abcd}{\\sqrt[xyz]{wxyz}}\n"
                   "\\begin{matrix}abcd&\\frac{efgh}{ijkl}\\\\mnop&\\sqrt{qrst}\\end{matrix}\n"
                   "{group}\\alpha tail\n\\frac{}{}");
    LayoutFile(file);
    const TextMeasure measure = [](std::string_view text, float size) {
        return text.size() * size * 0.5f;
    };
    const std::vector<CursorPath> positions = {
        {0, 1, 0, 0, 2},       // numerator
        {0, 1, 1, 1, 0, 0, 2}, // denominator's root index
        {0, 1, 1, 1, 1, 0, 2}, // denominator's root argument
        {1, 1, 0, 0, 0, 2},    // matrix upper left
        {1, 1, 0, 1, 1, 0, 0, 2}, // matrix upper right numerator
        {1, 1, 0, 1, 1, 1, 0, 2}, // matrix upper right denominator
        {1, 1, 1, 0, 0, 2},    // matrix lower left
        {1, 1, 1, 1, 1, 1, 0, 2}, // matrix lower right root
        {2, 1, 0, 2},          // bare group
        {2, 4, 2},             // text after symbol
        {3, 1, 0, 0, 0},       // empty numerator
        {3, 1, 1, 0, 0},       // empty denominator
    };
    for (const auto& path : positions) {
        SCOPED_TRACE(::testing::PrintToString(path));
        std::size_t offset = 0;
        const auto* text = file.CursorText(path, &offset);
        ASSERT_NE(text, nullptr);
        const auto caret = text->CaretBox(offset, &measure);
        ASSERT_TRUE(caret);
        ASSERT_TRUE(file.ClickAt(caret->x, caret->y + caret->height / 2, &measure));
        EXPECT_EQ(file.cursorPos, path);
        EXPECT_EQ(file.selectionStart, path);
    }
}

TEST_F(LineLayoutTest, ClickUsesScrolledBoundsAndUtf8CharacterBoundaries) {
    FileMpp file;
    file.SetSource("aé日z\n" + std::string(30, '\n') + "\\frac{a}{b}\nlast");
    LayoutFile(file);
    const TextMeasure measure = [](std::string_view text, float size) {
        return text.size() * size * 0.5f;
    };
    const auto first = Row(file, 0);
    // A click within a multibyte character must choose one of its ends.
    ASSERT_TRUE(file.ClickAt(first.x + 2 * 9, first.y + first.height / 2, &measure));
    EXPECT_TRUE(file.cursorPos.back() == 1 || file.cursorPos.back() == 3);
    auto scroll = Clay_GetScrollContainerData(CLAY_ID("Workspace"));
    ASSERT_TRUE(scroll.found);
    scroll.scrollPosition->y = -200;
    scroll.scrollPosition->x = -10;
    LayoutFile(file);
    const auto last = Row(file, file.LineCount() - 1);
    ASSERT_TRUE(file.ClickAt(last.x + 18, last.y + last.height / 2, &measure));
    EXPECT_EQ(file.cursorPos, (CursorPath{file.LineCount() - 1, 0, 2}));
}

TEST_F(LineLayoutTest, ClickCancelsPendingDeleteAndMatrixInsertWheel) {
    FileMpp file;
    file.SetSource("\\frac{a}{b}\n\\begin{matrix}x\\end{matrix}");
    LayoutFile(file);
    file.MoveCursorTo({0, 2, 0});
    ASSERT_TRUE(file.PressKey(KEY_BACKSPACE, false, nullptr));
    ASSERT_TRUE(file.GetPendingDelete());
    const auto row = Row(file, 0);
    ASSERT_TRUE(file.ClickAt(row.x, row.y, nullptr));
    EXPECT_FALSE(file.GetPendingDelete());
    file.MoveCursorTo({1, 1, 0, 0, 0, 0});
    ASSERT_TRUE(file.PressKey(KEY_ENTER, false, nullptr));
    ASSERT_TRUE(file.GetInsertWheel());
    ASSERT_TRUE(file.ClickAt(row.x, row.y, nullptr));
    EXPECT_FALSE(file.GetInsertWheel());
    EXPECT_FALSE(file.modified);
}

TEST_F(LineLayoutTest, ClickOnSymbolChoosesTextOnEitherSide) {
    FileMpp file;
    file.SetSource("before\\alpha after");
    LayoutFile(file);
    const auto symbol = LastLayoutBounds(file.content[0]->GetChild(1)->GetClayId());
    ASSERT_TRUE(symbol);
    ASSERT_TRUE(file.ClickAt(symbol->x + symbol->width * 0.25f,
                             symbol->y + symbol->height / 2, nullptr));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 0, 6}));
    ASSERT_TRUE(file.ClickAt(symbol->x + symbol->width * 0.75f,
                             symbol->y + symbol->height / 2, nullptr));
    EXPECT_EQ(file.cursorPos, (CursorPath{0, 2, 0}));
}

TEST_F(LineLayoutTest, ClickBeforeFirstLayoutLeavesCursorAlone) {
    FileMpp file;
    const auto cursor = file.cursorPos;
    EXPECT_FALSE(file.ClickAt(100, 100, nullptr));
    EXPECT_EQ(file.cursorPos, cursor);
}
