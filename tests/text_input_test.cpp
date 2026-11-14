// One-line text editing (TextBuffer with singleLine) and the TextInput box
// built on it. Runs without a window; layout uses a fake fixed-width font.

#include "editor/TextBuffer.h"
#include "raylib.h"
#include "ui/TextInput.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {
TextBuffer Line(const std::string& text, std::size_t cursor) {
    TextBuffer buffer(/*singleLine=*/true);
    buffer.Reset(text);
    buffer.cursorPos = buffer.selectionStart = cursor;
    return buffer;
}

// Each character is 10px wide.
class TextInputLayoutTest : public ::testing::Test {
protected:
    std::vector<char> memory_;
    Clay_Context* previousContext_ = nullptr;

    void SetUp() override {
        previousContext_ = Clay_GetCurrentContext();
        memory_.resize(Clay_MinMemorySize());
        Clay_Initialize(Clay_CreateArenaWithCapacityAndMemory(memory_.size(), memory_.data()),
                        {800, 600}, {});
        Clay_SetMeasureTextFunction(
            [](Clay_StringSlice text, Clay_TextElementConfig* config, void*) -> Clay_Dimensions {
                return {text.length * 10.0f, float(config->fontSize)};
            },
            nullptr);
    }

    void TearDown() override { Clay_SetCurrentContext(previousContext_); }

    void Lay(TextInput& input, float width) {
        Clay_BeginLayout();
        CLAY(CLAY_ID("Holder"), {.layout = {.sizing = {CLAY_SIZING_FIXED(width), CLAY_SIZING_FIT(0)}}}) {
            input.Layout({});
        }
        Clay_EndLayout(0);
    }

    static Clay_BoundingBox Bounds(const char* id, uint32_t part) {
        const Clay_String name = {.isStaticallyAllocated = false,
                                  .length = static_cast<int32_t>(std::string(id).size()),
                                  .chars = id};
        const auto data = Clay_GetElementData(Clay_GetElementIdWithIndex(name, part));
        EXPECT_TRUE(data.found);
        return data.boundingBox;
    }
};

constexpr uint32_t boxPart = 0, cursorPart = 2, selectedPart = 4;
}  // namespace

// --- Single-line editing ------------------------------------------------------------

TEST(SingleLineText, EnterUpAndDownAreLeftForTheOwner) {
    auto buffer = Line("abc", 1);
    EXPECT_FALSE(buffer.PressKey(KEY_ENTER, false, nullptr));
    EXPECT_FALSE(buffer.PressKey(KEY_UP, false, nullptr));
    EXPECT_FALSE(buffer.PressKey(KEY_DOWN, false, nullptr));
    EXPECT_EQ(buffer.text, "abc");
    EXPECT_EQ(buffer.cursorPos, 1u);
}

TEST(SingleLineText, PastedLineBreaksBecomeSpaces) {
    auto buffer = Line("ab", 1);
    ASSERT_TRUE(buffer.PasteText("x\r\ny\nz"));
    EXPECT_EQ(buffer.text, "ax y zb");
}

TEST(SingleLineText, EditsLikeTheTextEditor) {
    auto buffer = Line("", 0);
    for (const char c : std::string("sqrt")) {
        buffer.TypeCharacter(static_cast<char32_t>(c));
    }
    buffer.PressKey(KEY_LEFT, true, nullptr);
    buffer.PressKey(KEY_LEFT, true, nullptr);
    EXPECT_EQ(buffer.SelectedText(), "rt");
    buffer.TypeCharacter(U'X');
    EXPECT_EQ(buffer.text, "sqX");
    ASSERT_TRUE(buffer.Undo());
    EXPECT_EQ(buffer.text, "sqrt");
}

TEST(SingleLineText, ResetForgetsHistory) {
    auto buffer = Line("", 0);
    buffer.TypeCharacter(U'a');
    buffer.Reset("b");
    EXPECT_EQ(buffer.text, "b");
    EXPECT_EQ(buffer.cursorPos, 0u);
    EXPECT_FALSE(buffer.CanUndo());
}

// --- The box -----------------------------------------------------------------------------

TEST_F(TextInputLayoutTest, CursorSitsBetweenCharactersWithoutMovingText) {
    TextInput input("TestInput");
    input.buffer.Reset("abcd");
    input.buffer.cursorPos = input.buffer.selectionStart = 2;
    Lay(input, 200);
    const auto box = Bounds("TestInput", boxPart);
    EXPECT_FLOAT_EQ(Bounds("TestInput", cursorPart).x - box.x, 20.0f);
    EXPECT_FLOAT_EQ(Bounds("TestInput", cursorPart).width, 0.0f);
}

TEST_F(TextInputLayoutTest, SelectionCoversTheSelectedCharacters) {
    TextInput input("TestInput");
    input.buffer.Reset("abcdef");
    input.buffer.selectionStart = 1;
    input.buffer.cursorPos = 4;
    Lay(input, 200);
    const auto box = Bounds("TestInput", boxPart);
    const auto selected = Bounds("TestInput", selectedPart);
    EXPECT_FLOAT_EQ(selected.x - box.x, 10.0f);
    EXPECT_FLOAT_EQ(selected.width, 30.0f);
}

TEST_F(TextInputLayoutTest, LongTextScrollsToKeepTheCursorInView) {
    TextInput input("TestInput");
    input.buffer.Reset(std::string(50, 'x'));  // 500px in a 100px box
    input.buffer.cursorPos = input.buffer.selectionStart = 50;
    Lay(input, 100);
    Lay(input, 100);  // scrolling uses the previous frame's layout
    const auto box = Bounds("TestInput", boxPart);
    const float cursorX = Bounds("TestInput", cursorPart).x - box.x;
    EXPECT_GE(cursorX, 0.0f);
    EXPECT_LE(cursorX, box.width);

    input.buffer.cursorPos = input.buffer.selectionStart = 0;
    Lay(input, 100);
    Lay(input, 100);
    EXPECT_FLOAT_EQ(Bounds("TestInput", cursorPart).x, Bounds("TestInput", boxPart).x);
}
