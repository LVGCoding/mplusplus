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
    file.content = Parser().ParseText("plain\n\\frac{a}{b}\n\\sqrt[3]{x}\n"
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

    file.content = Parser().ParseText("\\frac{\\frac{a}{b}}{c}\nnext");
    LayoutFile(file);
    EXPECT_FLOAT_EQ(Label(1).y, Row(file, 0).y + Row(file, 0).height);
    file.content = Parser().ParseText("short\nnext");
    LayoutFile(file);
    EXPECT_FLOAT_EQ(Row(file, 0).height, editor_style::lineHeight);
    EXPECT_FLOAT_EQ(Label(1).y - Label(0).y, editor_style::lineHeight);
}

TEST_F(LineLayoutTest,
       EmptyRowsAndNestedNewlinesUseRenderedLineCount) {
    FileMpp file;
    file.content = Parser().ParseText("\n\\begin{matrix}a&b\\\\\nc&d\\end{matrix}\n\n");
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
    file.content = Parser().ParseText(source);
    LayoutFile(file);
    auto scroll = Clay_GetScrollContainerData(CLAY_ID("Workspace"));
    EXPECT_TRUE(scroll.found);
    const float previousY = Row(file, 10).y;
    scroll.scrollPosition->y = -100;
    LayoutFile(file);
    EXPECT_FLOAT_EQ(Row(file, 10).y, previousY - 100);
    EXPECT_FLOAT_EQ(Label(10).y, Row(file, 10).y);
}
