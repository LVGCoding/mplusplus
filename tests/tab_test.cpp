#include "editor/editor.h"
#include "ui/layout_tabs.h"

#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace {
using namespace layout_internal;

void AddTabs(Editor& editor) {
    editor.NewFile("a.txt");
    editor.NewFile("b.mpp");
    editor.NewFile("c.txt");
}

std::vector<const FileBase*> FileOrder(const Editor& editor) {
    std::vector<const FileBase*> files;
    for (const auto& file : editor.Files()) {
        files.push_back(file.get());
    }
    return files;
}

class TabDragTest : public ::testing::Test {
protected:
    std::vector<char> memory_;
    Clay_Context* previousContext_ = nullptr;

    void SetUp() override {
        previousContext_ = Clay_GetCurrentContext();
        memory_.resize(Clay_MinMemorySize());
        Clay_Initialize(
            Clay_CreateArenaWithCapacityAndMemory(memory_.size(), memory_.data()), {800, 600}, {});
        Clay_SetMeasureTextFunction(
            [](Clay_StringSlice text, Clay_TextElementConfig* config, void*) -> Clay_Dimensions {
                return {text.length * 10.0f, static_cast<float>(config->fontSize)};
            },
            nullptr);
        ResetState();
    }

    void TearDown() override {
        ResetState();
        Clay_SetCurrentContext(previousContext_);
    }

    void ResetState() {
        tabDrag = {};
        tabScroll.clear();
        selectTabRequested = closeTabRequested = SIZE_MAX;
        closeRequested = unsavedChangesOpen = false;
        CloseAllMenus();
    }

    void LayoutTabs(Editor& editor,
                    float width = 800) {
        Clay_BeginLayout();
        CLAY(CLAY_ID("TabTestRoot"),
             {.layout = {.sizing = {CLAY_SIZING_FIXED(width), CLAY_SIZING_FIXED(100)},
                         .layoutDirection = CLAY_TOP_TO_BOTTOM}}) {
            LayoutFileBar(editor);
        }
        Clay_EndLayout(0);
    }

    Vector2 TabCenter(std::size_t index) {
        const auto tab = Clay_GetElementData(CLAY_IDI("FileTab", static_cast<uint32_t>(index)));
        EXPECT_TRUE(tab.found);
        return {tab.boundingBox.x + tab.boundingBox.width / 2,
                tab.boundingBox.y + tab.boundingBox.height / 2};
    }
};
}  // namespace

TEST(EditorTabs,
     EveryMovePreservesTheActiveDocumentAndOtherTabsOrder) {
    for (std::size_t active = 0; active < 3; ++active) {
        for (std::size_t from = 0; from < 3; ++from) {
            for (std::size_t to = 0; to < 3; ++to) {
                Editor editor;
                AddTabs(editor);
                editor.SetActiveTab(active);
                const FileBase* activeFile = editor.ActiveFile();
                auto expected = FileOrder(editor);
                const auto moved = expected[from];
                expected.erase(expected.begin() + static_cast<std::ptrdiff_t>(from));
                expected.insert(expected.begin() + static_cast<std::ptrdiff_t>(to), moved);

                EXPECT_EQ(editor.MoveTab(from, to), from != to);
                EXPECT_EQ(FileOrder(editor), expected);
                EXPECT_EQ(editor.ActiveFile(), activeFile);
            }
        }
    }
}

TEST(EditorTabs,
     InvalidMovesLeaveTabsUnchanged) {
    Editor empty;
    EXPECT_FALSE(empty.MoveTab(0, 1));
    EXPECT_EQ(empty.ActiveFile(), nullptr);
    Editor editor;
    AddTabs(editor);
    const auto original = FileOrder(editor);
    const auto activeFile = editor.ActiveFile();
    EXPECT_FALSE(editor.MoveTab(3, 0));
    EXPECT_FALSE(editor.MoveTab(0, SIZE_MAX));
    EXPECT_EQ(FileOrder(editor), original);
    EXPECT_EQ(editor.ActiveFile(), activeFile);
}

TEST(EditorTabs,
     ReorderingPreservesUnsavedContentAndUndoHistory) {
    Editor editor;
    AddTabs(editor);
    auto* file = dynamic_cast<FileText*>(editor.ActiveFile());
    ASSERT_NE(file, nullptr);
    ASSERT_TRUE(file->PasteText("unsaved edits"));
    ASSERT_TRUE(editor.MoveTab(2, 0));
    EXPECT_EQ(editor.ActiveFile(), file);
    EXPECT_EQ(file->text, "unsaved edits");
    EXPECT_TRUE(file->modified);
    ASSERT_TRUE(file->Undo());
    EXPECT_TRUE(file->text.empty());
    editor.CloseActiveFile();
    EXPECT_EQ(editor.Files().size(), 2u);
    EXPECT_EQ(editor.ActiveFile()->path, "a.txt");
}

TEST_F(TabDragTest,
       DraggingAcrossTabsReordersInBothDirections) {
    Editor editor;
    AddTabs(editor);
    editor.SetActiveTab(0);
    const auto original = FileOrder(editor);
    tabScroll[original[0]] = {-10, -100};
    tabScroll[original[1]] = {0, -50};
    LayoutTabs(editor);
    BeginTabDrag(*editor.ActiveFile(), TabCenter(0));
    auto destination = TabCenter(2);
    destination.x += 1;
    UpdateTabDrag(editor, destination, true, false, 0);
    EXPECT_EQ(FileOrder(editor),
              (std::vector<const FileBase*>{original[1], original[2], original[0]}));
    EXPECT_EQ(editor.ActiveFile(), original[0]);

    LayoutTabs(editor);
    destination = TabCenter(0);
    destination.x -= 1;
    UpdateTabDrag(editor, destination, true, false, 0);
    EXPECT_EQ(FileOrder(editor), original);
    EXPECT_EQ(tabScroll[original[0]].y, -100);
    EXPECT_EQ(tabScroll[original[1]].y, -50);
    UpdateTabDrag(editor, destination, false, false, 0);
    EXPECT_EQ(tabDrag.file, nullptr);
}

TEST_F(TabDragTest,
       SmallPointerMovementRemainsAClick) {
    Editor editor;
    AddTabs(editor);
    LayoutTabs(editor);
    const auto original = FileOrder(editor);
    auto mouse = TabCenter(0);
    BeginTabDrag(*editor.Files()[0], mouse);
    mouse.x += 3;
    UpdateTabDrag(editor, mouse, true, false, 0);
    EXPECT_FALSE(tabDrag.dragging);
    EXPECT_EQ(FileOrder(editor), original);
    UpdateTabDrag(editor, mouse, false, false, 0);
    EXPECT_EQ(tabDrag.file, nullptr);
}

TEST_F(TabDragTest,
       AQuickDragUsesTheReleasePosition) {
    Editor editor;
    AddTabs(editor);
    LayoutTabs(editor);
    const auto file = editor.Files()[0].get();
    BeginTabDrag(*file, TabCenter(0));
    auto destination = TabCenter(2);
    destination.x += 1;
    UpdateTabDrag(editor, destination, false, false, 0);
    EXPECT_EQ(editor.Files()[2].get(), file);
    EXPECT_EQ(tabDrag.file, nullptr);
}

TEST_F(TabDragTest,
       EscapeMenusAndClosingCancelDragging) {
    Editor editor;
    AddTabs(editor);
    LayoutTabs(editor);
    const auto original = FileOrder(editor);
    auto destination = TabCenter(2);
    destination.x += 1;
    BeginTabDrag(*editor.Files()[0], TabCenter(0));
    UpdateTabDrag(editor, destination, true, true, 0);
    EXPECT_EQ(tabDrag.file, nullptr);
    BeginTabDrag(*editor.Files()[0], TabCenter(0));
    fileMenuOpen = true;
    UpdateTabDrag(editor, destination, true, false, 0);
    EXPECT_EQ(tabDrag.file, nullptr);
    fileMenuOpen = false;
    BeginTabDrag(*editor.Files()[0], TabCenter(0));
    closeRequested = true;
    UpdateTabDrag(editor, destination, true, false, 0);
    EXPECT_EQ(tabDrag.file, nullptr);
    EXPECT_EQ(FileOrder(editor), original);
}

TEST_F(TabDragTest,
       LeavingTheTabBarDoesNotReorderDocuments) {
    Editor editor;
    AddTabs(editor);
    LayoutTabs(editor);
    const auto original = FileOrder(editor);
    BeginTabDrag(*editor.Files()[0], TabCenter(0));
    auto destination = TabCenter(2);
    destination.y += 100;
    UpdateTabDrag(editor, destination, true, false, 0);
    EXPECT_EQ(FileOrder(editor), original);
}

TEST_F(TabDragTest,
       ClosingTheCapturedFileClearsTheDragSafely) {
    Editor editor;
    AddTabs(editor);
    editor.SetActiveTab(0);
    LayoutTabs(editor);
    BeginTabDrag(*editor.ActiveFile(), TabCenter(0));
    editor.CloseActiveFile();
    UpdateTabDrag(editor, {300, 10}, true, false, 0);
    EXPECT_EQ(tabDrag.file, nullptr);
    EXPECT_EQ(editor.Files().size(), 2u);
}

TEST_F(TabDragTest,
       DraggingNearAnEdgeScrollsAnOverflowingTabBar) {
    Editor editor;
    AddTabs(editor);
    LayoutTabs(editor, 200);
    BeginTabDrag(*editor.Files()[0], TabCenter(0));
    const auto bar = Clay_GetElementData(CLAY_ID("FileBar")).boundingBox;
    const Vector2 edge = {bar.x + bar.width - 1, bar.y + bar.height / 2};
    UpdateTabDrag(editor, edge, true, false, 1.0f);
    const auto scroll = Clay_GetScrollContainerData(CLAY_ID("FileBar"));
    ASSERT_TRUE(scroll.found);
    EXPECT_LT(scroll.scrollPosition->x, 0);
    EXPECT_GE(scroll.scrollPosition->x,
              scroll.scrollContainerDimensions.width - scroll.contentDimensions.width);
}
