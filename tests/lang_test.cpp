// The language pipeline (src/lang): lowering, parsing, checking and code
// generation. Inputs are written as document source (LaTeX), one row per
// line, and loaded the way the editor loads a file.

#include "editor/FileMpp.h"
#include "lang/Checker.h"
#include "lang/Compile.h"
#include "lang/Lower.h"
#include "lang/Parser.h"
#include "run/Run.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

using lang::Token;

FileMpp Document(const std::string& source) {
    FileMpp file;
    file.SetSource(source);
    return file;
}

// Tokens of a document, written compactly: words and numbers as they are,
// strings in quotes, structures as shape[slot | slot], rows ending in ⏎.
std::string DescribeTokens(const std::vector<Token>& tokens) {
    std::string text;
    for (const Token& token : tokens) {
        if (!text.empty() && token.kind != Token::Kind::End) {
            text += ' ';
        }
        switch (token.kind) {
            case Token::Kind::String: text += '"' + token.text + '"'; break;
            case Token::Kind::Newline: text += "⏎"; break;
            case Token::Kind::End: break;
            case Token::Kind::Structure: {
                static const char* SHAPES[] = {"?", "frac", "root", "pow", "group",
                                               "matrix", "graph", "table"};
                text += SHAPES[static_cast<int>(token.shape)];
                text += '[';
                for (std::size_t i = 0; i < token.slots.size(); ++i) {
                    text += (i ? " | " : "") + DescribeTokens(token.slots[i]);
                }
                text += ']';
                break;
            }
            default: text += token.text;
        }
    }
    return text;
}

std::string Lower(const std::string& source) {
    FileMpp file = Document(source);
    std::vector<lang::Diagnostic> diagnostics;
    auto tokens = lang::LowerDocument(file.content, diagnostics);
    EXPECT_TRUE(diagnostics.empty()) << diagnostics.front().message;
    return DescribeTokens(tokens);
}

// An expression as an S-expression, to check precedence: (+ 1 (⋅ 2 x)).
std::string DescribeExpr(const lang::Expr& expr) {
    using K = lang::Expr::Kind;
    auto list = [&](const std::string& head) {
        std::string text = "(" + head;
        for (const auto& arg : expr.args) {
            text += " " + DescribeExpr(*arg);
        }
        return text + ")";
    };
    switch (expr.kind) {
        case K::Number:
        case K::Name:
        case K::Bool:
        case K::Constant: return expr.text;
        case K::String: return '"' + expr.text + '"';
        case K::Infinity: return "∞";
        case K::Unary:
        case K::Binary: return list(expr.op);
        case K::Fraction: return list("frac");
        case K::Power: return list("^");
        case K::Root: return list("√");
        case K::Call: return list(expr.text);
        case K::Compare: {
            std::string text = "(cmp " + DescribeExpr(*expr.args[0]);
            for (std::size_t i = 0; i < expr.ops.size(); ++i) {
                text += " " + expr.ops[i] + " " + DescribeExpr(*expr.args[i + 1]);
            }
            return text + ")";
        }
    }
    return "?";
}

// Parse a document and describe its statements, or its first syntax error.
std::string ParseStatements(const std::string& source) {
    FileMpp file = Document(source);
    std::vector<lang::Diagnostic> diagnostics;
    auto tokens = lang::LowerDocument(file.content, diagnostics);
    lang::Program program = lang::Parse(tokens, diagnostics);
    if (!diagnostics.empty()) {
        return "error: " + diagnostics.front().message;
    }
    std::string text;
    for (const auto& statement : program.statements) {
        if (!text.empty()) {
            text += "; ";
        }
        switch (statement->kind) {
            case lang::Stmt::Kind::Define:
                text += statement->name +
                        (statement->declaredType.empty() ? "" : " ∈ " + statement->declaredType);
                if (statement->value) {
                    text += " := " + DescribeExpr(*statement->value);
                }
                break;
            case lang::Stmt::Kind::Assign:
                text += statement->name + " ← " + DescribeExpr(*statement->value);
                break;
            case lang::Stmt::Kind::Expr: text += DescribeExpr(*statement->value); break;
        }
    }
    return text;
}

std::string ParseExpression(const std::string& source) {
    return ParseStatements("print(" + source + ")");
}

std::vector<lang::Diagnostic> Diagnose(const std::string& source) {
    FileMpp file = Document(source);
    return lang::Compile(file.content, lang::CompileMode::CheckOnly).diagnostics;
}

// The first diagnostic's message, or "" if the document is fine.
std::string FirstError(const std::string& source) {
    const auto diagnostics = Diagnose(source);
    return diagnostics.empty() ? "" : diagnostics.front().message;
}

}  // namespace

// --- Lowering ----------------------------------------------------------------------

TEST(Lower, TextBecomesWordsNumbersAndOperators) {
    EXPECT_EQ(Lower("x := 2.5 + 3"), "x := 2.5 + 3 ⏎");
    EXPECT_EQ(Lower("total12 := .5e-3"), "total12 := .5e-3 ⏎");
}

TEST(Lower, OperatorsHaveOneSpellingHoweverTheyAreTyped) {
    EXPECT_EQ(Lower("a <- b * c - d"), "a ← b ⋅ c − d ⏎");
    EXPECT_EQ(Lower("a \\leftarrow b \\cdot c"), "a ← b ⋅ c ⏎");
    EXPECT_EQ(Lower("a <= b >= c != d"), "a ≤ b ≥ c ≠ d ⏎");
}

TEST(Lower, NumbersStopBeforeARange) {
    EXPECT_EQ(Lower("1..n"), "1 .. n ⏎");
}

TEST(Lower, NumberThenLetterIsTwoTokens) {
    EXPECT_EQ(Lower("2x"), "2 x ⏎");
    EXPECT_EQ(Lower("2e"), "2 e ⏎");
}

TEST(Lower, GreekLettersAreWords) {
    FileMpp file = Document("\\alpha := \\pi");
    std::vector<lang::Diagnostic> diagnostics;
    auto tokens = lang::LowerDocument(file.content, diagnostics);
    ASSERT_GE(tokens.size(), 3u);
    EXPECT_EQ(tokens[0].kind, Token::Kind::Word);
    EXPECT_EQ(tokens[0].text, "α");
    EXPECT_EQ(tokens[0].ident, "alpha");
    EXPECT_TRUE(tokens[0].greek);
    EXPECT_EQ(tokens[2].text, "π");
    EXPECT_EQ(tokens[2].ident, "pi");
    EXPECT_EQ(Lower("\\sigma\\omega\\omicron"), "σ ω ο ⏎");
}

TEST(Lower, StructuresBecomeOneTokenWithSlots) {
    EXPECT_EQ(Lower("\\frac{1}{x+1}"), "frac[1 | x + 1] ⏎");
    EXPECT_EQ(Lower("\\sqrt{2}"), "root[2] ⏎");
    EXPECT_EQ(Lower("\\sqrt[3]{x}"), "root[x | 3] ⏎");
    EXPECT_EQ(Lower("x\\exp{}{2}"), "x pow[ | 2] ⏎");
    EXPECT_EQ(Lower("\\exp{x}{2}"), "pow[x | 2] ⏎");
}

TEST(Lower, RowsEndWithANewline) {
    EXPECT_EQ(Lower("a := 1\nb := 2"), "a := 1 ⏎ b := 2 ⏎");
}

TEST(Lower, StringsKeepGreekLettersAndSymbols) {
    EXPECT_EQ(Lower("print(\"\\alpha \\leq 1\")"), "print ( \"α ≤ 1\" ) ⏎");
    EXPECT_EQ(Lower("\"x = \", x"), "\"x = \" , x ⏎");
}

TEST(Lower, ReportsAnUnclosedString) {
    FileMpp file = Document("print(\"oops)");
    std::vector<lang::Diagnostic> diagnostics;
    lang::LowerDocument(file.content, diagnostics);
    ASSERT_EQ(diagnostics.size(), 1u);
    EXPECT_NE(diagnostics[0].message.find("closing quote"), std::string::npos);
}

TEST(Lower, TokensKnowWhereTheyAre) {
    FileMpp file = Document("a := 1\nbb := \\frac{1}{x}");
    std::vector<lang::Diagnostic> diagnostics;
    auto tokens = lang::LowerDocument(file.content, diagnostics);
    // bb: row 1, text child 0, bytes 0..2.
    const Token& bb = tokens[4];
    EXPECT_EQ(bb.text, "bb");
    EXPECT_EQ(bb.ref.start, (CursorPath{1, 0, 0}));
    EXPECT_EQ(bb.ref.end, (CursorPath{1, 0, 2}));
    // The fraction covers child 1, from the end of the text before it.
    const Token& fraction = tokens[6];
    EXPECT_EQ(fraction.ref.start, (CursorPath{1, 0, 6}));
    EXPECT_EQ(fraction.ref.end, (CursorPath{1, 2, 0}));
    // The x in its denominator: row 1, child 1, slot 1, text 0, bytes 0..1.
    const Token& x = fraction.slots[1][0];
    EXPECT_EQ(x.ref.start, (CursorPath{1, 1, 1, 0, 0}));
    EXPECT_EQ(x.ref.end, (CursorPath{1, 1, 1, 0, 1}));
}

// --- Parsing -----------------------------------------------------------------------

TEST(Parse, Statements) {
    EXPECT_EQ(ParseStatements("x := 1\ny \\in \\mathbb{R} := 2\nz \\in \\mathbb{N}\nx <- 3"),
              "x := 1; y ∈ ℝ := 2; z ∈ ℕ; x ← 3");
    EXPECT_EQ(ParseStatements("a := 1; b := 2"), "a := 1; b := 2");
}

TEST(Parse, Precedence) {
    EXPECT_EQ(ParseExpression("1 + 2 * 3"), "(print (+ 1 (⋅ 2 3)))");
    EXPECT_EQ(ParseExpression("(1 + 2) * 3"), "(print (⋅ (+ 1 2) 3))");
    EXPECT_EQ(ParseExpression("-x^2"), "(print (− (^ x 2)))");
    EXPECT_EQ(ParseExpression("2^3^2"), "(print (^ 2 (^ 3 2)))");
    EXPECT_EQ(ParseExpression("a - b - c"), "(print (− (− a b) c))");
    EXPECT_EQ(ParseExpression("7 mod 3 + 1"), "(print (+ (mod 7 3) 1))");
    EXPECT_EQ(ParseExpression("\\neg a \\land b \\lor c"), "(print (∨ (∧ (¬ a) b) c))");
}

TEST(Parse, ComparisonsChain) {
    EXPECT_EQ(ParseExpression("0 \\leq x < n"), "(print (cmp 0 ≤ x < n))");
    EXPECT_EQ(ParseExpression("a = b \\land c \\neq d"), "(print (∧ (cmp a = b) (cmp c ≠ d)))");
}

TEST(Parse, Structures) {
    EXPECT_EQ(ParseExpression("\\frac{a+1}{2}"), "(print (frac (+ a 1) 2))");
    EXPECT_EQ(ParseExpression("\\sqrt{x}"), "(print (√ x))");
    EXPECT_EQ(ParseExpression("\\sqrt[3]{x}"), "(print (√ x 3))");
    EXPECT_EQ(ParseExpression("x\\exp{}{2} + 1"), "(print (+ (^ x 2) 1))");
    EXPECT_EQ(ParseExpression("\\exp{x+1}{2}"), "(print (^ (+ x 1) 2))");
}

TEST(Parse, ImplicitMultiplication) {
    EXPECT_EQ(ParseExpression("2x"), "(print (⋅ 2 x))");
    EXPECT_EQ(ParseExpression("2(x+1)"), "(print (⋅ 2 (+ x 1)))");
    EXPECT_EQ(ParseExpression("2x\\exp{}{2}"), "(print (⋅ 2 (^ x 2)))");
    EXPECT_EQ(ParseExpression("x\\sqrt{2}"), "(print (⋅ x (√ 2)))");
    EXPECT_EQ(ParseExpression("\\frac{1}{2}x"), "(print (⋅ (frac 1 2) x))");
    EXPECT_EQ(ParseExpression("(a+b)(a-b)"), "(print (⋅ (+ a b) (− a b)))");
    EXPECT_EQ(ParseExpression("x\\exp{}{2}y"), "(print (⋅ (^ x 2) y))");
}

TEST(Parse, NamesNextToEachOtherNeedAnOperator) {
    EXPECT_EQ(ParseExpression("a b"), "error: Write a⋅b to multiply a and b.");
    EXPECT_EQ(ParseExpression("\\alpha b"), "error: Write α⋅b to multiply α and b.");
    EXPECT_EQ(ParseExpression("sin x"), "error: Write sin(x) to use the function sin.");
    EXPECT_EQ(ParseExpression("2 3"), "error: Write ⋅ between these to multiply them.");
}

TEST(Parse, NameThenParenthesisIsACall) {
    EXPECT_EQ(ParseExpression("f(x, 2)"), "(print (f x 2))");
}

TEST(Parse, RowsContinueAfterAnOperatorOrInsideParentheses) {
    EXPECT_EQ(ParseStatements("x := 1 +\n2"), "x := (+ 1 2)");
    EXPECT_EQ(ParseStatements("print(1,\n2)"), "(print 1 2)");
}

TEST(Parse, ReportsSyntaxErrorsAndKeepsGoing) {
    FileMpp file = Document("x := \nprint(1))\ny := 2");
    std::vector<lang::Diagnostic> diagnostics;
    auto tokens = lang::LowerDocument(file.content, diagnostics);
    lang::Program program = lang::Parse(tokens, diagnostics);
    ASSERT_EQ(diagnostics.size(), 2u);
    EXPECT_EQ(diagnostics[0].message, "Expected a value, but found the end of the row.");
    EXPECT_EQ(diagnostics[0].ref.Row(), 0u);
    EXPECT_EQ(diagnostics[1].ref.Row(), 1u);
    ASSERT_EQ(program.statements.size(), 1u);
    EXPECT_EQ(program.statements[0]->name, "y");
}

TEST(Parse, EmptySlotsAreErrors) {
    EXPECT_EQ(ParseExpression("\\frac{}{2}"), "error: The numerator is empty.");
}

TEST(Parse, NotYetSupported) {
    EXPECT_NE(ParseStatements("if x").find("control flow"), std::string::npos);
    EXPECT_NE(ParseStatements("A := \\begin{pmatrix}1&2\\end{pmatrix}").find("Matrices"),
              std::string::npos);
}

// --- Checking ----------------------------------------------------------------------

TEST(Check, AcceptsAValidProgram) {
    EXPECT_EQ(FirstError("a := 1\nb \\in \\mathbb{R} := a\nb <- b + \\frac{a}{2}\n"
                         "ok := 0 \\leq b < 10 \\land \\neg (a = 2)\nprint(\"b =\", b, ok)"),
              "");
}

TEST(Check, UndefinedNames) {
    EXPECT_EQ(FirstError("print(x)"), "x isn't defined.");
    EXPECT_EQ(FirstError("print(y)\ny := 1"), "y is used before it is defined (row 2).");
    EXPECT_EQ(FirstError("a := 1\nb := 2\nprint(ab)"),
              "ab isn't defined. To multiply, write a⋅b.");
    EXPECT_EQ(FirstError("x <- 1"), "x isn't defined yet. To define it, write x := …");
}

TEST(Check, DefiningTwiceSuggestsTheArrow) {
    EXPECT_EQ(FirstError("x := 1\nx := 2"),
              "x is already defined (row 1). To change it, write x ← …");
}

TEST(Check, EqualsComparesInsteadOfAssigning) {
    EXPECT_EQ(FirstError("x := 1\nx = 2"),
              "= compares two values. To define x, write x := …, or to change it, x ← …");
}

TEST(Check, UnusedValues) {
    EXPECT_EQ(FirstError("1 + 2"), "This value isn't used. To show it, write print(…).");
}

TEST(Check, Types) {
    EXPECT_EQ(FirstError("n \\in \\mathbb{Z} := 1.5"),
              "n is in ℤ, but this is a real number (ℝ). Use floor(…), ceil(…) or round(…) "
              "to make it whole.");
    EXPECT_EQ(FirstError("n := 1\nn <- \\frac{1}{2}"),
              "n is in ℤ, but this is a real number (ℝ). Use floor(…), ceil(…) or round(…) "
              "to make it whole.");
    EXPECT_EQ(FirstError("n := 1\nn <- floor(\\frac{1}{2})"), "");
    EXPECT_EQ(FirstError("b := true + 1"), "+ needs numbers, but this is in 𝔹.");
    EXPECT_EQ(FirstError("b := 1 \\land true"), "∧ needs true/false values (𝔹), but this is in ℤ.");
    EXPECT_EQ(FirstError("b := true < 1"), "< needs numbers, but this is in 𝔹.");
    EXPECT_EQ(FirstError("b := \"a\" = 1"), "Can't compare Σ* with ℤ.");
}

TEST(Check, Functions) {
    EXPECT_EQ(FirstError("print(sin(1), max(1, 2.5), floor(2.5))"), "");
    EXPECT_EQ(FirstError("x := sin(1, 2)"), "sin takes 1 argument, not 2.");
    EXPECT_EQ(FirstError("x := f(1)"), "There's no function called f.");
    EXPECT_EQ(FirstError("x := 1\ny := x(2)"), "x isn't a function. To multiply, write x⋅(…).");
    EXPECT_EQ(FirstError("x := print(1)"), "print(…) doesn't give a value.");
    EXPECT_EQ(FirstError("sin := 1"), "sin is a built-in function; choose another name.");
}

TEST(Check, ConstantsCanBeShadowed) {
    EXPECT_EQ(FirstError("print(\\pi, e)"), "");
    EXPECT_EQ(FirstError("e := 2\nprint(e)"), "");
}

TEST(Check, BigNumbers) {
    EXPECT_EQ(FirstError("x := 99999999999999999999"),
              "This number is too big for ℤ. Write it with a decimal point "
              "(99999999999999999999.0) to make it a real number.");
}

TEST(Check, ErrorsPointAtTheProblem) {
    const auto diagnostics = Diagnose("a := 1\nb := a + c");
    ASSERT_EQ(diagnostics.size(), 1u);
    EXPECT_EQ(diagnostics[0].ref.start, (CursorPath{1, 0, 9}));
    EXPECT_EQ(diagnostics[0].ref.end, (CursorPath{1, 0, 10}));
}

// --- Runtime error lines ---------------------------------------------------------------

TEST(RuntimeErrorLine, Parses) {
    auto error = ParseRuntimeError("error:12: division by zero");
    ASSERT_TRUE(error);
    EXPECT_EQ(error->location, 12);
    EXPECT_EQ(error->message, "division by zero");
    EXPECT_EQ(ParseRuntimeError("error:-1: internal error: x")->location, -1);
    EXPECT_FALSE(ParseRuntimeError("errors are fun"));
    EXPECT_FALSE(ParseRuntimeError("error:12"));
}

// --- Diagnostics in the editor ------------------------------------------------------------

TEST(DocumentDiagnostics, AreClearedWhenTheContentChanges) {
    FileMpp file = Document("x := y");
    file.diagnostics = lang::Compile(file.content, lang::CompileMode::CheckOnly).diagnostics;
    ASSERT_EQ(file.diagnostics.size(), 1u);
    const auto version = file.ContentVersion();
    // Moving the cursor isn't a change.
    file.PressKey(KEY_RIGHT, false, nullptr);
    EXPECT_EQ(file.diagnostics.size(), 1u);
    EXPECT_EQ(file.ContentVersion(), version);
    file.TypeCharacter(U'1');
    EXPECT_TRUE(file.diagnostics.empty());
    EXPECT_GT(file.ContentVersion(), version);
    file.diagnostics.push_back({"stale", {}});
    file.Undo();
    EXPECT_TRUE(file.diagnostics.empty());
}
