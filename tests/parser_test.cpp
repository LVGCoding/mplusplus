#include "test_helpers.h"

#include <gtest/gtest.h>

using test::ParseAndDescribe;
using test::RoundTrip;

// --- Text -------------------------------------------------------------------

TEST(Parser, EmptyInputHasNoNodes) {
    EXPECT_EQ(ParseAndDescribe(""), "");
}

TEST(Parser, AdjacentTextIsMerged) {
    EXPECT_EQ(ParseAndDescribe("x + y = 2"), R"(text("x + y = 2"))");
}

TEST(Parser, UnknownCommandIsText) {
    EXPECT_EQ(ParseAndDescribe("a\\cdot b"), R"(text("a\cdot b"))");
}

// --- Greek ------------------------------------------------------------------

TEST(Parser, GreekLetter) {
    EXPECT_EQ(ParseAndDescribe("\\alpha"), "greek(alpha)");
}

TEST(Parser, EveryGreekLetterParses) {
    for (int i = GreekNode::alpha; i <= GreekNode::omega; ++i) {
        const auto symbol = static_cast<GreekNode::GreekSymbolType>(i);
        const std::string name(GreekNode::SymbolName(symbol));
        EXPECT_EQ(ParseAndDescribe("\\" + name), "greek(" + name + ")");
    }
}

TEST(Parser, GreekLetterBetweenText) {
    EXPECT_EQ(ParseAndDescribe("2\\pi r"), R"(text("2"), greek(pi), text(" r"))");
}

TEST(Parser, GreekPrefixIsNotGreek) {
    // \alphax is one unknown command, not \alpha followed by x.
    EXPECT_EQ(ParseAndDescribe("\\alphax"), R"(text("\alphax"))");
}

// --- Fractions --------------------------------------------------------------

TEST(Parser, Fraction) {
    EXPECT_EQ(ParseAndDescribe("\\frac{a}{b}"), R"(frac(line(text("a")), line(text("b"))))");
}

TEST(Parser, FractionWithEmptyArguments) {
    EXPECT_EQ(ParseAndDescribe("\\frac{}{}"), "frac(line(), line())");
}

TEST(Parser, NestedFraction) {
    EXPECT_EQ(ParseAndDescribe("\\frac{\\frac{1}{x}}{\\beta}"),
              R"(frac(line(frac(line(text("1")), line(text("x")))), line(greek(beta))))");
}

TEST(Parser, FractionAllowsSpaceBeforeArguments) {
    EXPECT_EQ(ParseAndDescribe("\\frac {a} {b}"), R"(frac(line(text("a")), line(text("b"))))");
}

TEST(Parser, FractionMissingArgumentIsText) {
    EXPECT_EQ(ParseAndDescribe("\\frac{a}"), R"(text("\frac"), group(text("a")))");
    EXPECT_EQ(ParseAndDescribe("\\frac"), R"(text("\frac"))");
}

// --- Square roots -----------------------------------------------------------

TEST(Parser, SquareRoot) {
    EXPECT_EQ(ParseAndDescribe("\\sqrt{x}"), R"(sqrt(line(text("x"))))");
}

TEST(Parser, NthRoot) {
    EXPECT_EQ(ParseAndDescribe("\\sqrt[3]{x}"), R"(sqrt[line(text("3"))](line(text("x"))))");
}

TEST(Parser, RootContainingBracesAndBrackets) {
    EXPECT_EQ(ParseAndDescribe("\\sqrt[{n]}]{x}"),
              R"(sqrt[line(group(text("n]")))](line(text("x"))))");
}

TEST(Parser, SqrtUnclosedRootIsText) {
    EXPECT_EQ(ParseAndDescribe("\\sqrt[3{x}"), R"(text("\sqrt[3"), group(text("x")))");
}

TEST(Parser, SqrtMissingArgumentIsText) {
    EXPECT_EQ(ParseAndDescribe("\\sqrt x"), R"(text("\sqrt x"))");
}

// --- Groups -----------------------------------------------------------------

TEST(Parser, BareGroup) {
    EXPECT_EQ(ParseAndDescribe("x^{2}"), R"(text("x^"), group(text("2")))");
}

TEST(Parser, UnclosedGroupIsText) {
    EXPECT_EQ(ParseAndDescribe("{a"), R"(text("{a"))");
}

TEST(Parser, StrayClosingBraceIsText) {
    EXPECT_EQ(ParseAndDescribe("a}b"), R"(text("a}b"))");
}

// --- Matrices ---------------------------------------------------------------

TEST(Parser, Matrix) {
    EXPECT_EQ(ParseAndDescribe("\\begin{matrix}a&b\\\\c&d\\end{matrix}"),
              R"(matrix[[line(text("a")), line(text("b"))], )"
              R"([line(text("c")), line(text("d"))]])");
}

TEST(Parser, MatrixVariantsKeepEnvironment) {
    for (const char* environment : {"pmatrix", "bmatrix", "Bmatrix", "vmatrix", "Vmatrix"}) {
        const std::string env = environment;
        EXPECT_EQ(ParseAndDescribe("\\begin{" + env + "}1\\end{" + env + "}"),
                  env + R"([[line(text("1"))]])");
    }
}

TEST(Parser, MatrixCellsCanContainNodes) {
    EXPECT_EQ(ParseAndDescribe("\\begin{pmatrix}\\frac{1}{2}&\\alpha\\end{pmatrix}"),
              R"(pmatrix[[line(frac(line(text("1")), line(text("2")))), line(greek(alpha))]])");
}

TEST(Parser, MatrixWithEmptyCells) {
    EXPECT_EQ(ParseAndDescribe("\\begin{matrix}&\\end{matrix}"), "matrix[[line(), line()]]");
}

TEST(Parser, NestedMatrix) {
    EXPECT_EQ(ParseAndDescribe(
                  "\\begin{bmatrix}\\begin{matrix}a\\end{matrix}&b\\end{bmatrix}"),
              R"(bmatrix[[line(matrix[[line(text("a"))]]), line(text("b"))]])");
}

TEST(Parser, AmpersandInsideGroupInMatrixStaysInCell) {
    EXPECT_EQ(ParseAndDescribe("\\begin{matrix}{a&b}\\end{matrix}"),
              R"(matrix[[line(group(text("a&b")))]])");
}

TEST(Parser, MatrixWithoutEndIsText) {
    EXPECT_EQ(ParseAndDescribe("\\begin{matrix}a&b"), R"(text("\begin"), group(text("matrix")), text("a&b"))");
}

TEST(Parser, MismatchedEndIsText) {
    EXPECT_EQ(ParseAndDescribe("\\begin{matrix}a\\end{pmatrix}"),
              R"(text("\begin"), group(text("matrix")), text("a\end"), group(text("pmatrix")))");
}

TEST(Parser, UnknownEnvironmentIsText) {
    EXPECT_EQ(ParseAndDescribe("\\begin{align}x\\end{align}"),
              R"(text("\begin"), group(text("align")), text("x\end"), group(text("align")))");
}

TEST(Parser, AmpersandOutsideMatrixIsText) {
    EXPECT_EQ(ParseAndDescribe("a & b"), R"(text("a & b"))");
}

// --- Round trips ------------------------------------------------------------
// Parsing then calling ToText() should give back the original source.

class ParserRoundTrip : public ::testing::TestWithParam<const char*> {};

TEST_P(ParserRoundTrip, ToTextReproducesSource) {
    EXPECT_EQ(RoundTrip(GetParam()), GetParam());
}

INSTANTIATE_TEST_SUITE_P(
    Sources,
    ParserRoundTrip,
    ::testing::Values("",
                      "plain text\nwith lines",
                      "x = \\frac{-b}{2a} + \\sqrt[3]{\\alpha}",
                      "\\frac{\\frac{1}{2}}{\\sqrt{\\pi}}",
                      "e^{i\\pi} + 1 = 0",
                      "\\begin{pmatrix} 1 & \\frac{1}{2} \\\\ \\theta & 0 \\end{pmatrix}",
                      "\\frac{a}",
                      "{unclosed",
                      "stray } and ] and &",
                      "\\begin{matrix}a\\end{pmatrix}",
                      "trailing backslash \\"));
