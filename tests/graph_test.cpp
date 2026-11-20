// Graphs: how their ranges and labels are stored, parsing them back, and
// editing the title, labels and functions inline. Runs without a window.

#include "editor/FileMpp.h"
#include "math/GraphNode.h"
#include "math/GraphSettings.h"
#include "math/Options.h"
#include "parser/parser.h"
#include "raylib.h"

#include <gtest/gtest.h>
#include <string>

namespace {
constexpr std::size_t TITLE = GraphNode::TITLE_SLOT;
constexpr std::size_t X_LABEL = GraphNode::X_LABEL_SLOT;
constexpr std::size_t FIRST_FUNCTION = GraphNode::FIRST_FUNCTION_SLOT;

const GraphNode* OnlyGraph(const std::vector<std::unique_ptr<EditorNode>>& nodes) {
    return nodes.size() == 1 ? dynamic_cast<const GraphNode*>(nodes[0].get()) : nullptr;
}

std::string RoundTrip(const std::string& source) {
    std::string text;
    for (const auto& node : Parser().ParseText(source)) {
        text += node->ToText();
    }
    return text;
}

FileMpp MakeDocument(const std::string& source,
                     CursorPath cursor) {
    FileMpp file;
    file.SetSource(source);
    file.MoveCursorTo(std::move(cursor));
    return file;
}

// Cursor in slot `slot` of the graph in row 0, at byte `offset` of its text.
// (Rows are text, graph, text after normalizing, so the graph is child 1.)
CursorPath InGraph(std::size_t slot,
                   std::size_t offset) {
    return {0, 1, slot, 0, offset};
}

GraphNode* GraphInRow0(FileMpp& file) {
    return dynamic_cast<GraphNode*>(file.NodeAt(CursorPath{0, 1}));
}
}  // namespace

// --- Settings ---------------------------------------------------------------------

TEST(GraphSettings, DefaultsWriteNoOptions) {
    EXPECT_EQ(GraphSettings().ToOptions(), "");
    EXPECT_EQ(GraphSettings::FromOptions({}), GraphSettings());
}

TEST(GraphSettings, RangesRoundTripAndUnknownOptionsAreKept) {
    const auto settings =
        GraphSettings::FromOptions(options::Parse("xmin=0, xmax=2.5, ymin=-1, style={a, b}"));
    EXPECT_EQ(settings.xMin, 0);
    EXPECT_EQ(settings.xMax, 2.5);
    EXPECT_EQ(settings.yMin, -1);
    EXPECT_EQ(settings.yMax, 10);
    EXPECT_EQ(settings.ToOptions(), "xmin=0, xmax=2.5, ymin=-1, style={a, b}");
}

TEST(GraphSettings, ScalesRoundTrip) {
    const auto settings = GraphSettings::FromOptions(options::Parse("xscale=log, yscale=cubic"));
    EXPECT_EQ(settings.xScale, "log");
    EXPECT_EQ(settings.yScale, "linear");  // not a scale: kept as unknown
    EXPECT_EQ(settings.ToOptions(), "xscale=log, yscale=cubic");
}

TEST(GraphSettings, BadValuesKeepDefaults) {
    const auto settings = GraphSettings::FromOptions(options::Parse("xmin=abc, ymax="));
    EXPECT_EQ(settings, GraphSettings());
}

// --- Parsing ----------------------------------------------------------------------

TEST(GraphParse, LabelsFunctionsAndRanges) {
    const auto nodes = Parser().ParseText(
        "\\begin{graph}[title={a, b}, xlabel=t, ylabel, xmin=0]y=x\\\\y=\\frac{1}{x}\\end{graph}");
    const GraphNode* graph = OnlyGraph(nodes);
    ASSERT_NE(graph, nullptr);
    EXPECT_EQ(graph->Title()->ToText(), "a, b");
    EXPECT_EQ(graph->XLabel()->ToText(), "t");
    EXPECT_EQ(graph->YLabel()->ToText(), "");  // removed
    EXPECT_EQ(graph->Settings().xMin, 0);
    EXPECT_EQ(graph->FunctionCount(), 2u);
    EXPECT_EQ(graph->GetFunction(1)->ToText(), "y=\\frac{1}{x}");
}

TEST(GraphParse, DefaultLabelsAreXAndY) {
    const auto nodes = Parser().ParseText("\\begin{graph}\\end{graph}");
    const GraphNode* graph = OnlyGraph(nodes);
    ASSERT_NE(graph, nullptr);
    EXPECT_EQ(graph->Title()->ToText(), "");
    EXPECT_EQ(graph->XLabel()->ToText(), "x");
    EXPECT_EQ(graph->YLabel()->ToText(), "y");
}

TEST(GraphParse, RoundTrips) {
    for (const std::string source : {
             "\\begin{graph}y=x^2\\end{graph}",
             "\\begin{graph}\\end{graph}",
             "\\begin{graph}[title=Growth, xlabel=t, ylabel, xmin=0, xmax=5]a\\\\b\\end{graph}",
             "\\begin{graph}[title={\\frac{1}{2}}]\\end{graph}",
             "\\begin{graph}[][1,2]\\end{graph}",
             "\\begin{graph}[width=300, grid=false]\\end{graph}",  // older options are kept
         }) {
        EXPECT_EQ(RoundTrip(source), source) << source;
    }
}

TEST(GraphParse, MalformedStaysText) {
    for (const std::string source : {
             "\\begin{graph}[title=a y\\end{graph}",  // unclosed [
             "\\begin{graph}y=x",                     // no \end
             "\\begin{graph}a&b\\end{graph}",         // & is not allowed
         }) {
        EXPECT_EQ(OnlyGraph(Parser().ParseText(source)), nullptr) << source;
        EXPECT_EQ(RoundTrip(source), source) << source;
    }
}

// --- Editing ------------------------------------------------------------------------

TEST(GraphEdit, EnterAddsFunctionAndBackspaceRemovesIt) {
    FileMpp file = MakeDocument("\\begin{graph}x\\end{graph}", InGraph(FIRST_FUNCTION, 1));
    EXPECT_TRUE(file.PressKey(KEY_ENTER, false, nullptr));
    EXPECT_EQ(file.ContentText(), "\\begin{graph}x\\\\\\end{graph}");
    EXPECT_EQ(file.cursorPos, InGraph(FIRST_FUNCTION + 1, 0));

    EXPECT_TRUE(file.PressKey(KEY_BACKSPACE, false, nullptr));
    EXPECT_EQ(file.ContentText(), "\\begin{graph}x\\end{graph}");
    EXPECT_EQ(file.cursorPos, InGraph(FIRST_FUNCTION, 1));
}

TEST(GraphEdit, EnterInTheTitleAddsNothing) {
    FileMpp file = MakeDocument("\\begin{graph}x\\end{graph}", InGraph(TITLE, 0));
    file.PressKey(KEY_ENTER, false, nullptr);
    EXPECT_EQ(file.ContentText(), "\\begin{graph}x\\end{graph}");
}

TEST(GraphEdit, TitleAndLabelsAreTypedInPlace) {
    FileMpp file = MakeDocument("\\begin{graph}x\\end{graph}", InGraph(TITLE, 0));
    file.TypeCharacter(U'T');
    EXPECT_EQ(file.ContentText(), "\\begin{graph}[title=T]x\\end{graph}");

    // Deleting the x label's "x" removes the label.
    file.MoveCursorTo(InGraph(X_LABEL, 1));
    file.PressKey(KEY_BACKSPACE, false, nullptr);
    EXPECT_EQ(file.ContentText(), "\\begin{graph}[title=T, xlabel]x\\end{graph}");
}

TEST(GraphEdit, UpAndDownMoveThroughTheSlots) {
    FileMpp file = MakeDocument("\\begin{graph}a\\\\b\\end{graph}", InGraph(FIRST_FUNCTION, 1));
    file.PressKey(KEY_DOWN, false, nullptr);
    EXPECT_EQ(file.cursorPos[2], FIRST_FUNCTION + 1);
    file.PressKey(KEY_UP, false, nullptr);
    file.PressKey(KEY_UP, false, nullptr);
    EXPECT_EQ(file.cursorPos[2], X_LABEL);
}

TEST(GraphEdit, EditNodeChangesRangesAsOneUndoStep) {
    FileMpp file = MakeDocument("text\n\\begin{graph}x\\end{graph}", {0, 0, 2});
    auto* graph = dynamic_cast<GraphNode*>(file.NodeAt(CursorPath{1, 1}));
    ASSERT_NE(graph, nullptr);
    const std::uint32_t id = graph->GetId();
    ASSERT_EQ(file.FindNode(id), (CursorPath{1, 1}));

    EXPECT_TRUE(file.EditNode(id, [](EditorNode& node) {
        auto& target = dynamic_cast<GraphNode&>(node);
        GraphSettings settings = target.Settings();
        settings.xMax = 3;
        target.SetSettings(settings);
        return true;
    }));
    EXPECT_EQ(file.ContentText(), "text\n\\begin{graph}[xmax=3]x\\end{graph}");
    EXPECT_TRUE(file.modified);

    EXPECT_TRUE(file.Undo());
    EXPECT_EQ(file.ContentText(), "text\n\\begin{graph}x\\end{graph}");
    // The id survives undo, so the settings menu can still find the graph.
    EXPECT_EQ(file.FindNode(id), (CursorPath{1, 1}));
}

TEST(GraphEdit, EditNodeWithNoChangeAddsNoUndoStep) {
    FileMpp file = MakeDocument("\\begin{graph}x\\end{graph}", InGraph(FIRST_FUNCTION, 0));
    const std::uint32_t id = GraphInRow0(file)->GetId();
    EXPECT_FALSE(file.EditNode(id, [](EditorNode&) { return false; }));
    EXPECT_FALSE(file.CanUndo());
    EXPECT_FALSE(file.EditNode(12345, [](EditorNode&) { return true; }));
}
