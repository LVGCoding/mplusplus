// Tables: storage (title, header row), parsing them back, the "+" buttons,
// and editing cells and the title. Runs without a window.

#include "editor/FileMpp.h"
#include "math/MatrixNode.h"
#include "math/TableNode.h"
#include "parser/parser.h"
#include "raylib.h"

#include <gtest/gtest.h>
#include <string>

namespace {
const TableNode* OnlyTable(const std::vector<std::unique_ptr<EditorNode>>& nodes) {
    return nodes.size() == 1 ? dynamic_cast<const TableNode*>(nodes[0].get()) : nullptr;
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

// Cursor in cell (row, column) of the table in row 0, at byte `offset`.
// (Rows are text, table, text after normalizing, so the table is child 1.)
CursorPath InCell(std::size_t row,
                  std::size_t column,
                  std::size_t offset) {
    return {0, 1, row, column, 0, offset};
}

TableNode* TableInRow0(FileMpp& file) {
    return dynamic_cast<TableNode*>(file.NodeAt(CursorPath{0, 1}));
}
}  // namespace

// Parsing 

TEST(TableParse, TitleHeaderAndCells) {
    const auto nodes = Parser().ParseText(
        "\\begin{table}[title=Squares, header=true]x&x^2\\\\1&\\frac{1}{2}\\end{table}");
    const TableNode* table = OnlyTable(nodes);
    ASSERT_NE(table, nullptr);
    EXPECT_EQ(table->Title()->ToText(), "Squares");
    EXPECT_TRUE(table->HasHeader());
    EXPECT_EQ(table->RowCount(), 2u);
    EXPECT_EQ(table->ColumnCount(), 2u);
    EXPECT_EQ(table->GetCell(1, 1)->ToText(), "\\frac{1}{2}");
}

TEST(TableParse, RoundTrips) {
    for (const std::string source : {
             "\\begin{table}a&b\\\\c&d\\end{table}",
             "\\begin{table}\\end{table}",
             "\\begin{table}[title={a, b}, header=true]x&y\\end{table}",
             "\\begin{table}[autofill=true, f2=x^2]x&\\end{table}",  // unknown options are kept
         }) {
        EXPECT_EQ(RoundTrip(source), source) << source;
    }
}

TEST(TableParse, ShortRowsArePadded) {
    EXPECT_EQ(RoundTrip("\\begin{table}a&b\\\\c\\end{table}"),
              "\\begin{table}a&b\\\\c&\\end{table}");
}

TEST(TableParse, MatricesAreNotTables) {
    const auto nodes = Parser().ParseText("\\begin{matrix}a\\end{matrix}");
    ASSERT_EQ(nodes.size(), 1u);
    EXPECT_NE(dynamic_cast<const MatrixNode*>(nodes[0].get()), nullptr);
    EXPECT_EQ(dynamic_cast<const TableNode*>(nodes[0].get()), nullptr);
}

TEST(TableParse, MalformedStaysText) {
    for (const std::string source : {
             "\\begin{table}[title=a b\\end{table}",  // unclosed [
             "\\begin{table}a&b",                     // no \end
         }) {
        EXPECT_EQ(OnlyTable(Parser().ParseText(source)), nullptr) << source;
        EXPECT_EQ(RoundTrip(source), source) << source;
    }
}

// The "+" buttons and the header row 

TEST(TableActions, AddRowsColumnsAndAHeader) {
    TableNode table;  // 3x3, no header
    EXPECT_TRUE(table.RunAction(TableNode::ADD_ROW));
    EXPECT_EQ(table.RowCount(), 4u);
    EXPECT_TRUE(table.RunAction(TableNode::ADD_COLUMN));
    EXPECT_EQ(table.ColumnCount(), 4u);
    EXPECT_FALSE(table.HasHeader());
    EXPECT_TRUE(table.RunAction(TableNode::ADD_HEADER));
    EXPECT_TRUE(table.HasHeader());
    EXPECT_EQ(table.RowCount(), 5u);
    EXPECT_FALSE(table.RunAction(TableNode::ADD_HEADER));  // already has one
    EXPECT_FALSE(table.RunAction(99));
}

TEST(TableActions, RowsNeverGoAboveTheHeader) {
    const auto nodes = Parser().ParseText("\\begin{table}[header=true]x\\\\1\\end{table}");
    auto table = std::unique_ptr<TableNode>(static_cast<TableNode*>(nodes[0]->Clone().release()));
    EXPECT_EQ(table->InsertRow(0), 1u);
    EXPECT_EQ(table->ToText(), "\\begin{table}[header=true]x\\\\\\\\1\\end{table}");
}

TEST(TableActions, RemovingTheHeaderRowRemovesTheHeader) {
    const auto nodes = Parser().ParseText("\\begin{table}[header=true]x\\\\1\\end{table}");
    auto table = std::unique_ptr<TableNode>(static_cast<TableNode*>(nodes[0]->Clone().release()));
    EXPECT_TRUE(table->RemoveRow(0));
    EXPECT_FALSE(table->HasHeader());
    EXPECT_EQ(table->ToText(), "\\begin{table}1\\end{table}");
}

// Editing 

TEST(TableEdit, EveryCellIsEditable) {
    FileMpp file = MakeDocument("\\begin{table}&\\\\&\\end{table}", InCell(0, 0, 0));
    file.TypeCharacter(U'a');
    file.PressKey(KEY_RIGHT, false, nullptr);
    file.TypeCharacter(U'b');
    file.PressKey(KEY_DOWN, false, nullptr);
    file.TypeCharacter(U'd');
    file.PressKey(KEY_LEFT, false, nullptr);
    file.PressKey(KEY_LEFT, false, nullptr);
    file.TypeCharacter(U'c');
    EXPECT_EQ(file.ContentText(), "\\begin{table}a&b\\\\c&d\\end{table}");
}

TEST(TableEdit, UpFromTheTopRowEditsTheTitle) {
    FileMpp file = MakeDocument("\\begin{table}a&b\\end{table}", InCell(0, 1, 0));
    EXPECT_TRUE(file.PressKey(KEY_UP, false, nullptr));
    EXPECT_EQ(file.cursorPos[2], TableNode::TITLE_ROW);
    file.TypeCharacter(U'T');
    EXPECT_EQ(file.ContentText(), "\\begin{table}[title=T]a&b\\end{table}");
    file.PressKey(KEY_DOWN, false, nullptr);
    EXPECT_EQ(file.cursorPos[2], 0u);  // back in the top row
}

TEST(TableEdit, EnterOpensTheInsertWheelLikeAMatrix) {
    FileMpp file = MakeDocument("\\begin{table}a&b\\end{table}", InCell(0, 0, 1));
    EXPECT_TRUE(file.PressKey(KEY_ENTER, false, nullptr));
    EXPECT_TRUE(file.GetInsertWheel().has_value());
}

TEST(TableEdit, InsertWheelAboveTheHeaderAddsTheRowBelowIt) {
    FileMpp file = MakeDocument("\\begin{table}[header=true]x\\\\1\\end{table}", InCell(0, 0, 1));
    file.PressKey(KEY_ENTER, false, nullptr);  // open the wheel
    file.PressKey(KEY_UP, false, nullptr);     // choose "row above"
    file.PressKey(KEY_ENTER, false, nullptr);  // insert
    EXPECT_EQ(file.ContentText(), "\\begin{table}[header=true]x\\\\\\\\1\\end{table}");
    EXPECT_EQ(file.cursorPos, InCell(1, 0, 0));  // in the new row, under the header
}

TEST(TableEdit, PlusButtonIsOneUndoStep) {
    FileMpp file = MakeDocument("\\begin{table}a&b\\end{table}", {0, 0, 0});
    const std::uint32_t id = TableInRow0(file)->GetId();
    EXPECT_TRUE(
        file.EditNode(id, [](EditorNode& node) { return node.RunAction(TableNode::ADD_HEADER); }));
    EXPECT_EQ(file.ContentText(), "\\begin{table}[header=true]&\\\\a&b\\end{table}");
    EXPECT_TRUE(file.Undo());
    EXPECT_EQ(file.ContentText(), "\\begin{table}a&b\\end{table}");
}
