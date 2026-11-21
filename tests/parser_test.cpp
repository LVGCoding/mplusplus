#include "test_helpers.h"

#include <gtest/gtest.h>

using test::ParseAndDescribe;
using test::RoundTrip;

// --- Text -------------------------------------------------------------------

TEST(Parser,
     EmptyInputHasNoNodes) {
    EXPECT_EQ(ParseAndDescribe(""), "");
}

TEST(Parser,
     AdjacentTextIsMerged) {
    EXPECT_EQ(ParseAndDescribe("x + y = 2"), R"(text("x + y = 2"))");
}

TEST(Parser,
     UnknownCommandIsText) {
    EXPECT_EQ(ParseAndDescribe("a\\foo b"), R"(text("a\foo b"))");
}

// --- Greek ------------------------------------------------------------------

TEST(Parser,
     GreekLetter) {
    EXPECT_EQ(ParseAndDescribe("\\alpha"), "greek(alpha)");
}

TEST(Parser,
     EveryGreekLetterParses) {
    for (int i = GreekNode::GREEK_ALPHA; i <= GreekNode::GREEK_OMEGA; ++i) {
        const auto symbol = static_cast<GreekNode::GreekSymbolType>(i);
        const std::string name(GreekNode::SymbolName(symbol));
        EXPECT_EQ(ParseAndDescribe("\\" + name), "greek(" + name + ")");
    }
}

TEST(Parser,
     GreekLetterBetweenText) {
    // The space only ends the command word, as in TeX.
    EXPECT_EQ(ParseAndDescribe("2\\pi r"), R"(text("2"), greek(pi), text("r"))");
}

TEST(Parser,
     OnlyOneSpaceEndsACommandWord) {
    EXPECT_EQ(ParseAndDescribe("\\alpha  b"), R"(greek(alpha), text(" b"))");
}

TEST(Parser,
     LetterAfterGreekRoundTrips) {
    // Saving \alpha followed by b used to give \alphab, an unknown command.
    LineNode line;
    line.AddChild(std::make_unique<GreekNode>(GreekNode::GREEK_ALPHA));
    line.AddChild(std::make_unique<TextNode>("b"));
    EXPECT_EQ(line.ToText(), "\\alpha b");
    EXPECT_EQ(ParseAndDescribe(line.ToText()), R"(greek(alpha), text("b"))");
}

TEST(Parser,
     SpacesAfterCommandWordsRoundTrip) {
    for (const char* source : {"2\\pi r", "\\alpha  b", "\\in x", "x\\in\\mathbb{R}", "\\pi (x)",
                               "\\theta & 0", "\\alpha ",
                               "\\mathbb{R}x", "\\pi(x)", "\\alpha\\beta"}) {
        EXPECT_EQ(RoundTrip(source), source);
    }
}

// --- Symbols ----------------------------------------------------------------

TEST(Parser,
     Symbols) {
    EXPECT_EQ(ParseAndDescribe("x \\in \\mathbb{R}"),
              R"(text("x "), symbol(in), text(" "), symbol(mathbb{R}))");
    EXPECT_EQ(ParseAndDescribe("x \\in y"), R"(text("x "), symbol(in), text("y"))");
    EXPECT_EQ(ParseAndDescribe("x\\leftarrow 1"), R"(text("x"), symbol(leftarrow), text(" 1"))");
}

TEST(Parser,
     EverySymbolParses) {
    for (const auto& symbol : SymbolNode::Symbols()) {
        const std::string name(symbol.name);
        EXPECT_EQ(ParseAndDescribe("\\" + name), "symbol(" + name + ")");
        EXPECT_EQ(RoundTrip("\\" + name), "\\" + name);
    }
}

TEST(Parser,
     UnknownBlackboardLetterIsText) {
    EXPECT_EQ(ParseAndDescribe("\\mathbb{X}"), R"(text("\mathbb"), group(text("X")))");
}

TEST(Parser,
     GreekPrefixIsNotGreek) {
    // \alphax is one unknown command, not \alpha followed by x.
    EXPECT_EQ(ParseAndDescribe("\\alphax"), R"(text("\alphax"))");
}

// --- Fractions --------------------------------------------------------------

TEST(Parser,
     Fraction) {
    EXPECT_EQ(ParseAndDescribe("\\frac{a}{b}"), R"(frac(line(text("a")), line(text("b"))))");
}

TEST(Parser,
     FractionWithEmptyArguments) {
    EXPECT_EQ(ParseAndDescribe("\\frac{}{}"), "frac(line(), line())");
}

TEST(Parser,
     NestedFraction) {
    EXPECT_EQ(ParseAndDescribe("\\frac{\\frac{1}{x}}{\\beta}"),
              R"(frac(line(frac(line(text("1")), line(text("x")))), line(greek(beta))))");
}

TEST(Parser,
     FractionAllowsSpaceBeforeArguments) {
    EXPECT_EQ(ParseAndDescribe("\\frac {a} {b}"), R"(frac(line(text("a")), line(text("b"))))");
}

TEST(Parser,
     FractionMissingArgumentIsText) {
    EXPECT_EQ(ParseAndDescribe("\\frac{a}"), R"(text("\frac"), group(text("a")))");
    EXPECT_EQ(ParseAndDescribe("\\frac"), R"(text("\frac"))");
}

// --- Exponents -------------------------------------------------------------

TEST(Parser,
     Exponent) {
    // Like LaTeX: the exponent alone; the base is what comes before it.
    EXPECT_EQ(ParseAndDescribe("x^{2}"), R"(text("x"), sup(line(text("2"))))");
    EXPECT_EQ(ParseAndDescribe("^{2}"), R"(sup(line(text("2"))))");
}

TEST(Parser,
     ExponentWithoutBracesIsText) {
    EXPECT_EQ(ParseAndDescribe("2^10"), R"(text("2^10"))");
    EXPECT_EQ(ParseAndDescribe("a\\^{b}"), R"(text("a^"), group(text("b")))");
}

TEST(Parser,
     OldExponentFormPutsTheBaseBefore) {
    EXPECT_EQ(ParseAndDescribe("\\exp{}{2}"), R"(sup(line(text("2"))))");
    EXPECT_EQ(ParseAndDescribe("y=\\exp{x}{2}"), R"(text("y=x"), sup(line(text("2"))))");
    EXPECT_EQ(ParseAndDescribe("\\exp {x} {2}"), R"(text("x"), sup(line(text("2"))))");
    // A base of more than one term keeps its meaning with parentheses.
    EXPECT_EQ(ParseAndDescribe("\\exp{a+b}{2}"), R"x(text("(a+b)"), sup(line(text("2"))))x");
    EXPECT_EQ(ParseAndDescribe("\\exp{\\frac{1}{2}}{n}"),
              R"(frac(line(text("1")), line(text("2"))), sup(line(text("n"))))");
}

TEST(Parser,
     ExponentRoundTrips) {
    for (const std::string text : {"x^{2}", "e^{-x}+1", "2^3", "a_{i}^{2}", "a\\^{b}"}) {
        EXPECT_EQ(RoundTrip(text), text);
    }
}

TEST(Parser,
     ExponentMissingArgumentIsText) {
    EXPECT_EQ(ParseAndDescribe("\\exp"), R"(text("\exp"))");
    EXPECT_EQ(ParseAndDescribe("\\exp x"), R"(text("\exp x"))");
    EXPECT_EQ(ParseAndDescribe("x^"), R"(text("x^"))");
}

// --- Square roots -----------------------------------------------------------

TEST(Parser,
     SquareRoot) {
    EXPECT_EQ(ParseAndDescribe("\\sqrt{x}"), R"(sqrt(line(text("x"))))");
}

TEST(Parser,
     NthRoot) {
    EXPECT_EQ(ParseAndDescribe("\\sqrt[3]{x}"), R"(sqrt[line(text("3"))](line(text("x"))))");
}

TEST(Parser,
     RootContainingBracesAndBrackets) {
    EXPECT_EQ(ParseAndDescribe("\\sqrt[{n]}]{x}"),
              R"(sqrt[line(group(text("n]")))](line(text("x"))))");
}

TEST(Parser,
     SqrtUnclosedRootIsText) {
    EXPECT_EQ(ParseAndDescribe("\\sqrt[3{x}"), R"(text("\sqrt[3"), group(text("x")))");
}

TEST(Parser,
     SqrtMissingArgumentIsText) {
    EXPECT_EQ(ParseAndDescribe("\\sqrt x"), R"(text("\sqrt x"))");
}

// --- Groups -----------------------------------------------------------------

TEST(Parser,
     BareGroup) {
    EXPECT_EQ(ParseAndDescribe("x+{2}"), R"(text("x+"), group(text("2")))");
}

TEST(Parser,
     UnclosedGroupIsText) {
    EXPECT_EQ(ParseAndDescribe("{a"), R"(text("{a"))");
}

TEST(Parser,
     StrayClosingBraceIsText) {
    EXPECT_EQ(ParseAndDescribe("a}b"), R"(text("a}b"))");
}

// --- Matrices ---------------------------------------------------------------

TEST(Parser,
     Matrix) {
    EXPECT_EQ(ParseAndDescribe("\\begin{matrix}a&b\\\\c&d\\end{matrix}"),
              R"(matrix[[line(text("a")), line(text("b"))], )"
              R"([line(text("c")), line(text("d"))]])");
}

TEST(Parser,
     MatrixVariantsKeepEnvironment) {
    for (const char* environment : {"pmatrix", "bmatrix", "Bmatrix", "vmatrix", "Vmatrix"}) {
        const std::string environmentName = environment;
        EXPECT_EQ(
            ParseAndDescribe("\\begin{" + environmentName + "}1\\end{" + environmentName + "}"),
            environmentName + R"([[line(text("1"))]])");
    }
}

TEST(Parser,
     MatrixCellsCanContainNodes) {
    EXPECT_EQ(ParseAndDescribe("\\begin{pmatrix}\\frac{1}{2}&\\alpha\\end{pmatrix}"),
              R"(pmatrix[[line(frac(line(text("1")), line(text("2")))), line(greek(alpha))]])");
}

TEST(Parser,
     MatrixWithEmptyCells) {
    EXPECT_EQ(ParseAndDescribe("\\begin{matrix}&\\end{matrix}"), "matrix[[line(), line()]]");
}

TEST(Parser,
     NestedMatrix) {
    EXPECT_EQ(ParseAndDescribe("\\begin{bmatrix}\\begin{matrix}a\\end{matrix}&b\\end{bmatrix}"),
              R"(bmatrix[[line(matrix[[line(text("a"))]]), line(text("b"))]])");
}

TEST(Parser,
     AmpersandInsideGroupInMatrixStaysInCell) {
    EXPECT_EQ(ParseAndDescribe("\\begin{matrix}{a&b}\\end{matrix}"),
              R"(matrix[[line(group(text("a&b")))]])");
}

TEST(Parser,
     MatrixWithoutEndIsText) {
    EXPECT_EQ(ParseAndDescribe("\\begin{matrix}a&b"),
              R"(text("\begin"), group(text("matrix")), text("a&b"))");
}

TEST(Parser,
     MismatchedEndIsText) {
    EXPECT_EQ(ParseAndDescribe("\\begin{matrix}a\\end{pmatrix}"),
              R"(text("\begin"), group(text("matrix")), text("a\end"), group(text("pmatrix")))");
}

TEST(Parser,
     UnknownEnvironmentIsText) {
    EXPECT_EQ(ParseAndDescribe("\\begin{align}x\\end{align}"),
              R"(text("\begin"), group(text("align")), text("x\end"), group(text("align")))");
}

TEST(Parser,
     AmpersandOutsideMatrixIsText) {
    EXPECT_EQ(ParseAndDescribe("a & b"), R"(text("a & b"))");
}

// --- Round trips ------------------------------------------------------------
// Parsing then calling ToText() should give back the original source.

class ParserRoundTrip : public ::testing::TestWithParam<const char*> {};

TEST_P(ParserRoundTrip,
       ToTextReproducesSource) {
    EXPECT_EQ(RoundTrip(GetParam()), GetParam());
}

// A brace that doesn't pair up is kept as text, and text braces are saved
// escaped, so the saved form reads back as the same text.
TEST(Parser,
     UnpairedBracesBecomeEscapedText) {
    EXPECT_EQ(RoundTrip("{unclosed"), "\\{unclosed");
    EXPECT_EQ(RoundTrip("stray }"), "stray \\}");
    EXPECT_EQ(RoundTrip("\\{unclosed"), "\\{unclosed");
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
                      "stray ] and &",
                      "\\{typed braces\\}",
                      "x_{i} + \\norm{v} and \"a\\_b\"",
                      "\\sum_{k=1}^{n} k + \\bigcup_{i \\in S}^{} A_{i}",
                      "\\begin{matrix}a\\end{pmatrix}",
                      "trailing backslash \\"));
