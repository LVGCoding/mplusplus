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
                static const char* SHAPES[] = {"?",      "frac",  "root",  "pow",   "group",
                                               "matrix", "cases", "delim", "sub",   "bigop",
                                               "graph",  "table"};
                text += SHAPES[static_cast<int>(token.shape)];
                if (token.shape == Token::Shape::Delimited) {
                    text += ":" + token.text;
                }
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
        case K::Range: return list("..");
        case K::Matrix: return list(expr.text + " " + std::to_string(expr.rows) + "×" +
                                    std::to_string(expr.columns));
        case K::Index: return list("_");
        case K::All: return "*";
        case K::Transpose: return list("⊤");
        case K::Set: return list("set");
        case K::EmptySet: return "∅";
        case K::Builder:
            return "(set " + DescribeExpr(*expr.args[1]) + " : " + expr.var + " ∈ " +
                   DescribeExpr(*expr.args[0]) +
                   (expr.conditions[0] ? ", " + DescribeExpr(*expr.conditions[0]) : "") + ")";
        case K::TypeName: return expr.text;
        case K::StructType: {
            std::string text = "⟨";
            for (std::size_t i = 0; i < expr.fields.size(); ++i) {
                const lang::Param& field = expr.fields[i];
                text += (i ? ", " : "") + field.name + " ∈ " + field.declaredType +
                        (field.list ? "*" : "");
                for (const auto& tag : field.tags) {
                    text += " @" + tag.name;
                }
            }
            return text + "⟩";
        }
        case K::StructValue: {
            std::string text = "⟨";
            for (std::size_t i = 0; i < expr.ops.size(); ++i) {
                text += (i ? ", " : "") + expr.ops[i] + " = " + DescribeExpr(*expr.args[i]);
            }
            return text + "⟩";
        }
        case K::ListOf: return "(" + DescribeExpr(*expr.args[0]) + ")*";
        case K::List: return list("list");
        case K::Member: return "(. " + DescribeExpr(*expr.args[0]) + " " + expr.text + ")";
        case K::Method: return list("." + expr.text);
        case K::Template: {
            std::string text = "(template \"" + expr.ops[0] + "\"";
            for (std::size_t i = 0; i < expr.args.size(); ++i) {
                text += " " + DescribeExpr(*expr.args[i]) + " \"" + expr.ops[i + 1] + "\"";
            }
            return text + ")";
        }
        case K::BigOp:
        case K::Quantifier:
            return "(" + expr.text + " " + expr.var + " ∈ " + DescribeExpr(*expr.args[0]) + " " +
                   DescribeExpr(*expr.args[1]) + ")";
        case K::Cases: {
            std::string text = "(cases";
            for (std::size_t i = 0; i < expr.args.size(); ++i) {
                text += " " + DescribeExpr(*expr.args[i]) + " if " +
                        (expr.conditions[i] ? DescribeExpr(*expr.conditions[i]) : "otherwise");
            }
            return text + ")";
        }
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

std::string DescribeBlock(const lang::Block& block);

std::string DescribeStatement(const lang::Stmt& statement) {
    using K = lang::Stmt::Kind;
    switch (statement.kind) {
        case K::Define: {
            std::string text = statement.name;
            if (!statement.indices.empty()) {
                text += "_{";
                for (std::size_t i = 0; i < statement.indices.size(); ++i) {
                    text += (i ? "," : "") + DescribeExpr(*statement.indices[i]);
                }
                text += "}";
            }
            if (!statement.declaredType.empty()) {
                text += " ∈ " + statement.declaredType;
                if (!statement.dims.empty()) {
                    text += "^";
                    for (std::size_t i = 0; i < statement.dims.size(); ++i) {
                        text += (i ? "×" : "") +
                                (statement.dims[i] ? DescribeExpr(*statement.dims[i]) : "*");
                    }
                }
            }
            if (statement.value) {
                text += " := " + DescribeExpr(*statement.value);
            }
            return text;
        }
        case K::Assign: return statement.name + " ← " + DescribeExpr(*statement.value);
        case K::Expr: return DescribeExpr(*statement.value);
        case K::If: {
            std::string text =
                "if " + DescribeExpr(*statement.value) + " " + DescribeBlock(statement.body);
            if (statement.hasElse) {
                text += " else " + DescribeBlock(statement.elseBody);
            }
            return text;
        }
        case K::While:
            return "while " + DescribeExpr(*statement.value) + " " + DescribeBlock(statement.body);
        case K::For:
            return "for " + statement.name + " ∈ " + DescribeExpr(*statement.value) + " " +
                   DescribeBlock(statement.body);
        case K::Break: return "break";
        case K::Continue: return "continue";
        case K::Return:
            return statement.value ? "return " + DescribeExpr(*statement.value) : "return";
        case K::Function: {
            std::string text = statement.name + "(";
            for (std::size_t i = 0; i < statement.params.size(); ++i) {
                const lang::Param& param = statement.params[i];
                text += (i ? ", " : "") + param.name +
                        (param.declaredType.empty() ? "" : " ∈ " + param.declaredType);
            }
            text += ")";
            if (!statement.declaredType.empty()) {
                text += " → " + statement.declaredType;
            }
            if (statement.value) {
                return text + " := " + DescribeExpr(*statement.value);
            }
            return text + " " + DescribeBlock(statement.body);
        }
    }
    return "?";
}

std::string DescribeBlock(const lang::Block& block) {
    std::string text = "{";
    for (std::size_t i = 0; i < block.size(); ++i) {
        text += (i ? "; " : " ") + DescribeStatement(*block[i]);
    }
    return text + " }";
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
        text += DescribeStatement(*statement);
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

TEST(Lower, SubscriptsAndMatrices) {
    EXPECT_EQ(Lower("x_{i+1}"), "x sub[i + 1] ⏎");
    EXPECT_EQ(Lower("\\begin{pmatrix}1&2\\end{pmatrix}"), "matrix[1 | 2] ⏎");
}

TEST(Parse, VectorsAndMatrices) {
    EXPECT_EQ(ParseExpression("\\begin{pmatrix}1&2\\\\3&4\\end{pmatrix}"),
              "(print (pmatrix 2×2 1 2 3 4))");
    EXPECT_EQ(ParseExpression("A_{i,*} + v_{2..n}"), "(print (+ (_ A i *) (_ v (.. 2 n))))");
    EXPECT_EQ(ParseExpression("A\\exp{}{\\top} \\cdot x_{1}\\exp{}{2}"),
              "(print (⋅ (⊤ A) (^ (_ x 1) 2)))");
    EXPECT_EQ(ParseExpression("A \\setminus b"), "(print (∖ A b))");
    EXPECT_EQ(ParseStatements("v \\in \\mathbb{R}\\exp{}{3}"), "v ∈ ℝ^3");
    EXPECT_EQ(ParseStatements("A \\in \\exp{\\mathbb{R}}{n \\times m}"), "A ∈ ℝ^n×m");
    EXPECT_EQ(ParseStatements("v \\in \\mathbb{R}\\exp{}{*}"), "v ∈ ℝ^*");
    EXPECT_EQ(ParseStatements("\\alpha_{n} := 1"), "α_{n} := 1");
    EXPECT_EQ(ParseExpression("\\begin{pmatrix}1&2\\\\3\\end{pmatrix}"),
              "error: Every row of a matrix needs the same number of entries.");
}

TEST(Check, VectorsAndMatrices) {
    const std::string setup = "A := \\begin{pmatrix}1&2\\\\3&4\\end{pmatrix}\n"
                              "v := \\begin{pmatrix}1\\\\2\\end{pmatrix}\n";
    EXPECT_EQ(FirstError(setup + "print(A \\cdot v, v \\cdot v, 2A, A\\exp{}{-1}, A_{1,*})"), "");
    EXPECT_EQ(FirstError(setup + "print(A + v)"), "Can't add a vector to a matrix.");
    EXPECT_EQ(FirstError(setup + "print(v \\cdot A)"),
              "A vector (a column) can't be multiplied by a matrix. Did you mean v^⊤⋅A?");
    EXPECT_EQ(FirstError(setup + "print(A_{1})"), "A matrix has two indices, as in A_{i,j}.");
    EXPECT_EQ(FirstError(setup + "print(v_{1.5})"),
              "An index must be a whole number (ℤ), but this is in ℝ.");
    EXPECT_EQ(FirstError(setup + "print(A < A)"),
              "< can't compare vectors or matrices; = ≠ and ≈ can.");
    EXPECT_EQ(FirstError(setup + "print(det(v))"), "det needs a matrix, but this is in ℝ^n.");
    EXPECT_EQ(FirstError("n \\in \\mathbb{Z}\\exp{}{3}"),
              "Vectors and matrices hold real numbers: write ℝ^… .");
    EXPECT_EQ(FirstError("x := 2\nprint(x_{1})"), "x is a single value (ℤ), so it has no entries.");
    EXPECT_EQ(FirstError("print(I_{3} + 0_{3 \\times 3}, 1_{2})"), "");
}

TEST(Parse, Sets) {
    EXPECT_EQ(ParseExpression("\\{1, 2, 5..9\\}"), "(print (set 1 2 (.. 5 9)))");
    EXPECT_EQ(ParseExpression("\\{\\}"), "(print ∅)");
    EXPECT_EQ(ParseExpression("\\{x \\in S : x > 1\\}"), "(print (set x : x ∈ S, (cmp x > 1)))");
    EXPECT_EQ(ParseExpression("\\{2k | k \\in 1..n, k \\neq 3\\}"),
              "(print (set (⋅ 2 k) : k ∈ (.. 1 n), (cmp k ≠ 3)))");
    // ∩ binds tighter than ∪, and both tighter than ∈.
    EXPECT_EQ(ParseExpression("x \\in A \\cup B \\cap C"),
              "(print (cmp x ∈ (∪ A (∩ B C))))");
    EXPECT_EQ(ParseExpression("\\forall x \\in S : x > 0 \\land x < 9"),
              "(print (∀ x ∈ S (∧ (cmp x > 0) (cmp x < 9))))");
    EXPECT_EQ(ParseExpression("\\sum_{k=1}^{n} k\\exp{}{2} + 1"),
              "(print (+ (sum k ∈ (.. 1 n) (^ k 2)) 1))");
    EXPECT_EQ(ParseExpression("\\prod_{k \\in S}^{} 2k"), "(print (prod k ∈ S (⋅ 2 k)))");
    EXPECT_EQ(ParseExpression("\\sum_{k=1}^{} k"),
              "error: This needs an upper limit, as in k=1 up to n.");
    EXPECT_EQ(ParseExpression("\\{2k : k > 1\\}"),
              "error: Expected a variable and where it comes from, as in {x² : x ∈ S}.");
    // A block after a condition is still a block.
    EXPECT_EQ(ParseStatements("if x \\in S \\{ print(x) \\}"), "if (cmp x ∈ S) { (print x) }");
}

TEST(Check, Sets) {
    EXPECT_EQ(FirstError("A := \\{1, 2\\}\nB := \\{0.5\\} \\cup A\nprint(A \\subseteq B, |B|, "
                         "\\sum_{x \\in B}^{} x)"),
              "");
    EXPECT_EQ(FirstError("print(\\{1, \"a\"\\})"),
              "A set's elements must all be numbers or all be strings.");
    EXPECT_EQ(FirstError("print(\\{1\\} \\cup 2)"), "∪ needs two sets, but this is in ℤ.");
    EXPECT_EQ(FirstError("print(\\{1\\} + \\{2\\})"), "To join two sets, write A ∪ B.");
    EXPECT_EQ(FirstError("print(\"a\" \\in \\{1\\})"), "Can't look for Σ* among ℤ.");
    EXPECT_EQ(FirstError("print(\\sum_{k=1}^{3} \"a\")"), "Σ needs numbers, but this is in Σ*.");
    EXPECT_EQ(FirstError("print(\\forall k \\in 1..3 : k)"),
              "After ∀ x ∈ S : comes a condition, true or false (𝔹); this is in ℤ.");
    // Bound variables may reuse names from outside, and don't leak out.
    EXPECT_EQ(FirstError("k := 5\nprint(\\sum_{k=1}^{3} k, k)"), "");
    EXPECT_EQ(FirstError("E := \\emptyset\nE \\leftarrow E \\cup \\{1\\}"), "");
}

TEST(Parse, Data) {
    EXPECT_EQ(ParseExpression("\"Hi $\\{name\\}, $\\{n + 1\\}!\""),
              "(print (template \"Hi \" name \", \" (+ n 1) \"!\"))");
    EXPECT_EQ(ParseExpression("p.name + q.f(1, 2)"), "(print (+ (. p name) (.f q 1 2)))");
    EXPECT_EQ(ParseExpression("<name = \"Bob\", age = 4>"), "(print ⟨name = \"Bob\", age = 4⟩)");
    EXPECT_EQ(ParseExpression("\\begin{angle}x = 1\\\\y = 2\\end{angle}"),
              "(print ⟨x = 1, y = 2⟩)");
    EXPECT_EQ(ParseStatements("P := \\begin{angle}name \\in \\Sigma * @label(\"N\")\\end{angle}*"),
              "P := (⟨name ∈ Σ* @label⟩)*");
    EXPECT_EQ(ParseStatements("L := \\mathbb{R}*"), "L := (ℝ)*");
    EXPECT_EQ(ParseExpression("[1, 2] + m[\"k\"]"), "(print (+ (list 1 2) (_ m \"k\")))");
    EXPECT_EQ(ParseExpression("\"lo\" in s"), "(print (cmp \"lo\" in s))");
    EXPECT_EQ(ParseExpression("\\begin{angle}x = 1, y \\in \\mathbb{R}\\end{angle}"),
              "error: Write all the fields with ∈ (a struct type) or all with = (a value).");
}

TEST(Check, Data) {
    const std::string point = "Point := \\begin{angle}x \\in \\mathbb{R}, y \\in \\mathbb{R}"
                              "\\end{angle}\n";
    EXPECT_EQ(FirstError(point + "p \\in Point := <x = 1, y = 2>\np.x \\leftarrow 3\n"
                                 "print(p.x + p.y)"),
              "");
    EXPECT_EQ(FirstError(point + "p \\in Point := <x = 1>"),
              "This leaves out y: give it a value, or the field a @default(…).");
    EXPECT_EQ(FirstError(point + "p \\in Point := <x = 1, y = 2, z = 3>"),
              "Point has no field called z.");
    EXPECT_EQ(FirstError(point + "p \\in Point := <x = 1, y = 2>\nprint(p.z)"),
              "Point has no field called z.");
    EXPECT_EQ(FirstError("L := \\mathbb{Z}*\nL.push(\"a\")"), "L is in ℤ*, but this is in Σ*.");
    EXPECT_EQ(FirstError("L := \\mathbb{Z}*\nx := L.push(1)"), "push doesn't give a value.");
    EXPECT_EQ(FirstError("m \\in \\mathbb{R}\\exp{}{\\Sigma *}\nm[1] \\leftarrow 2"),
              "The keys of m are in Σ*, but this is in ℤ.");
    EXPECT_EQ(FirstError("print(\"a\" + 1)"),
              "+ joins two strings. To join other values, use a template: \"x = ${x}\".");
    EXPECT_EQ(FirstError("print(1 in \"a\")"),
              "in looks for text in a string (\"lo\" in \"hello\"); for sets and lists, write ∈.");
    EXPECT_EQ(FirstError("x \\in Thing"), "Thing isn't a type. Define it first, as in Thing := ⟨name ∈ Σ*⟩.");
    EXPECT_EQ(FirstError("T := \\begin{angle}x \\in \\mathbb{R} @colour(\"red\")\\end{angle}"),
              "There's no tag @colour. Tags are @label, @align, @filler, @format, @prefix, "
              "@postfix, @unit, @hidden and @default.");
    EXPECT_EQ(FirstError("T := \\begin{angle}x \\in \\mathbb{R} @default(\"a\")\\end{angle}"),
              "@default(a) doesn't fit x, which is in ℝ.");
    EXPECT_EQ(FirstError("s \\in \\{\"a\", \"b\"\\} := 3"), "s is in Σ*, but this is in ℤ.");
}

TEST(Check, Graphs) {
    EXPECT_EQ(FirstError("\\begin{graph} y=x\\exp{}{2} \\\\ sin(x) \\end{graph}"), "");
    // The x axis's label names the variable.
    EXPECT_EQ(FirstError("\\begin{graph}[xlabel=t] y=t + 1 \\end{graph}"), "");
    EXPECT_EQ(FirstError("\\begin{graph} y=\"a\" \\end{graph}"),
              "A graph's line is a function of x, like y = x², or data, like (xs, ys); this is "
              "in Σ*.");
    EXPECT_EQ(FirstError("s := [\"a\"]\n\\begin{graph} (s, s) \\end{graph}"),
              "Data in a graph is a vector or a list of numbers, but this is in Σ**.");
    EXPECT_EQ(FirstError("x := 1 + \\begin{graph} y=x \\end{graph}"),
              "A graph goes on a row of its own.");
}

TEST(Check, Figures) {
    EXPECT_EQ(FirstError("f := Figure()\nf.Line<x = [1], y = [2]>\nf.Scatter<x = [1], y = [2], "
                         "shape = \"square\">"),
              "");
    EXPECT_EQ(FirstError("f := Figure<colour = \"red\">"),
              "Figure has no setting colour; it has title, xlabel, ylabel, scaleX, scaleY.");
    EXPECT_EQ(FirstError("f := Figure()\nf.Line<x = [1]>"), "Line needs y = ….");
    EXPECT_EQ(FirstError("f := Figure()\nf.Bar<values = [\"a\"]>"),
              "values is a vector or a list of numbers, but this is in Σ**.");
    EXPECT_EQ(FirstError("f := Figure()\nf.Pie<values = [1]>"),
              "A Figure has Line, Scatter, Histogram and Bar.");
}

TEST(Check, SubscriptsAreLabelsOrIndices) {
    // An undefined name in the subscript is part of the name.
    EXPECT_EQ(FirstError("v_{max} := 3\nprint(v_{max})"), "");
    EXPECT_EQ(FirstError("v_{max} := 3\nprint(v)"), "v isn't defined.");
    // A defined name or a number makes it an index: an element of a family.
    EXPECT_EQ(FirstError("x_{1} := 3\nn := 1\nprint(x_{n}, x)"), "");
    EXPECT_EQ(FirstError("x := 1\nx_{1} := 3"),
              "x is already defined (row 1) as a single value, so it can't have elements.");
    EXPECT_EQ(FirstError("s_{1} := \"a\"\nprint(s)"),
              "s is a family of Σ*; use its elements, as in s_1.");
    EXPECT_EQ(FirstError("x_{1} := 3\nx := 2"),
              "x is already a family (row 1), with elements like x_1; choose another name.");
}

TEST(Lower, BracesDelimitersAndCases) {
    EXPECT_EQ(Lower("if x \\{ y \\}"), "if x { y } ⏎");
    EXPECT_EQ(Lower("\\abs{x} \\floor{y}"), "delim:abs[x] delim:floor[y] ⏎");
    EXPECT_EQ(Lower("\\begin{cases}1 & x > 0\\\\0 & otherwise\\end{cases}"),
              "cases[1 | x > 0 | 0 | otherwise] ⏎");
}

TEST(Parse, Blocks) {
    EXPECT_EQ(ParseStatements("if x > 0 \\{\nprint(x)\n\\} else \\{\nprint(0)\n\\}"),
              "if (cmp x > 0) { (print x) } else { (print 0) }");
    // else may start the row after the }, and else-if chains.
    EXPECT_EQ(ParseStatements("if a \\{ b() \\}\nelse if c \\{ d() \\}\nelse \\{ e() \\}"),
              "if a { (b) } else { if c { (d) } else { (e) } }");
    EXPECT_EQ(ParseStatements("while x < 3 \\{ x <- x + 1; print(x) \\}"),
              "while (cmp x < 3) { x ← (+ x 1); (print x) }");
}

TEST(Parse, ForLoopsOverRanges) {
    EXPECT_EQ(ParseStatements("for i \\in 1..n \\{ print(i) \\}"),
              "for i ∈ (.. 1 n) { (print i) }");
    EXPECT_EQ(ParseStatements("for i in 10..-2..0 \\{ break \\}"),
              "for i ∈ (.. 10 (− 2) 0) { break }");
    EXPECT_EQ(ParseStatements("for i \\in S \\{ \\}"), "for i ∈ S { }");
}

TEST(Parse, Functions) {
    EXPECT_EQ(ParseStatements("f(x) := x + 1"), "f(x) := (+ x 1)");
    EXPECT_EQ(ParseStatements("f(n \\in \\mathbb{N}, x) -> \\mathbb{R} := n \\cdot x"),
              "f(n ∈ ℕ, x) → ℝ := (⋅ n x)");
    EXPECT_EQ(ParseStatements("f(x) \\{\nreturn x\n\\}"), "f(x) { return x }");
    EXPECT_EQ(ParseStatements("g() \\{ return \\}"), "g() { return }");
    // Calls are still calls.
    EXPECT_EQ(ParseStatements("f(x)"), "(f x)");
    EXPECT_EQ(ParseStatements("f(x) := \\{"),
              "error: For a function written over several rows, leave out :=, as in f(…) { … }");
}

TEST(Parse, CasesAndDelimiters) {
    EXPECT_EQ(ParseExpression("\\begin{cases}1 & x > 0\\\\0 & otherwise\\end{cases}"),
              "(print (cases 1 if (cmp x > 0) 0 if otherwise))");
    EXPECT_EQ(ParseExpression("\\begin{cases}1 & if x > 0\\\\0 & else\\end{cases}"),
              "(print (cases 1 if (cmp x > 0) 0 if otherwise))");
    EXPECT_EQ(ParseExpression("\\abs{x} + \\floor{\\frac{a}{b}} + |y - 1|"),
              "(print (+ (+ (abs x) (floor (frac a b))) (abs (− y 1))))");
}

TEST(Parse, UnbalancedBraces) {
    EXPECT_EQ(ParseStatements("if x \\{\nprint(1)"),
              "error: This { is never closed: put a } at the end of the block.");
    EXPECT_EQ(ParseStatements("print(1)\n\\}"), "error: This } has no { before it to close.");
    EXPECT_EQ(ParseStatements("else \\{ \\}"), "error: else goes right after the } of an if.");
}

TEST(Parse, AnErrorInsideABlockKeepsTheBlock) {
    FileMpp file = Document("while true \\{\nx := \nbreak\n\\}\ny := 1");
    std::vector<lang::Diagnostic> diagnostics;
    auto tokens = lang::LowerDocument(file.content, diagnostics);
    lang::Program program = lang::Parse(tokens, diagnostics);
    ASSERT_EQ(diagnostics.size(), 1u);
    EXPECT_EQ(diagnostics[0].ref.Row(), 1u);
    ASSERT_EQ(program.statements.size(), 2u);
    EXPECT_EQ(program.statements[0]->body.size(), 1u);
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

TEST(Check, ControlFlow) {
    EXPECT_EQ(FirstError("x := 1\nif x \\{ \\}"),
              "The condition after if must be true or false (𝔹), but this is in ℤ.");
    EXPECT_EQ(FirstError("break"), "break can only be used inside a loop (while or for).");
    EXPECT_EQ(FirstError("return 1"), "return can only be used inside a function.");
    EXPECT_EQ(FirstError("for i \\in 1..3 \\{ i <- 2 \\}"),
              "i counts the loop, so it can't be changed inside it.");
    EXPECT_EQ(FirstError("for i \\in 1..\"a\" \\{ \\}"),
              "A range needs numbers, but this is in Σ*.");
    EXPECT_EQ(FirstError("x := 2\nfor i \\in x \\{ \\}"),
              "for needs a range (1..n), a set or a vector to go through, but this is in ℤ.");
}

TEST(Check, BlocksHaveTheirOwnNames) {
    EXPECT_EQ(FirstError("if true \\{ y := 1 \\}\nprint(y)"),
              "y isn't defined here: it is defined inside a block (row 1) and only exists there.");
    EXPECT_EQ(FirstError("if true \\{ y := 1 \\}\nif true \\{ y := 2 \\}"), "");
    EXPECT_EQ(FirstError("x := 1\nif true \\{ x := 2 \\}"),
              "x is already defined (row 1). To change it, write x ← …");
}

TEST(Check, UserFunctions) {
    EXPECT_EQ(FirstError("f(x) := x\nprint(f(1), f(2.5))"), "");
    EXPECT_EQ(FirstError("print(f(1))\nf(x) := x"), "f is used before it is defined (row 2).");
    EXPECT_EQ(FirstError("f(x) := x\nprint(f(1, 2))"), "f takes 1 argument, not 2.");
    EXPECT_EQ(FirstError("f(n \\in \\mathbb{Z}) := n\nprint(f(1.5))"),
              "n is in ℤ, but this is a real number (ℝ). Use floor(…), ceil(…) or round(…) to "
              "make it whole.");
    EXPECT_EQ(FirstError("g() \\{ print(1) \\}\nx := g()"), "g(…) doesn't give a value.");
    EXPECT_EQ(FirstError("f(x) := x\nf := 2"),
              "f is already a function (row 1); choose another name.");
    EXPECT_EQ(FirstError("if true \\{\nf(x) := x\n\\}"),
              "Functions are defined outside of blocks and other functions.");
    EXPECT_EQ(FirstError("f(x) \\{\nif x > 0 \\{ return 1 \\}\nreturn \"no\"\n\\}\nprint(f(1))"),
              "This returns Σ*, but f returns ℤ before (row 2).");
    // A function sees the globals defined before it, and its parameters.
    EXPECT_EQ(FirstError("a := 2\nf(x) := a \\cdot x\nprint(f(3))"), "");
    EXPECT_EQ(FirstError("f(x) := a \\cdot x\na := 2\nprint(f(3))"),
              "a is used before it is defined (row 2).");
    // Parameters may reuse a global's name.
    EXPECT_EQ(FirstError("x := 1\nf(x) := 2x\nprint(f(3))"), "");
}

TEST(Check, AProblemInAFunctionAlsoMarksTheCall) {
    const auto diagnostics = Diagnose("f(x) := x mod 2\nprint(f(\"a\"))\nprint(f(\"b\"))");
    ASSERT_EQ(diagnostics.size(), 2u);
    EXPECT_EQ(diagnostics[0].message, "mod needs numbers, but this is in Σ*.");
    EXPECT_EQ(diagnostics[0].ref.Row(), 0u);
    EXPECT_EQ(diagnostics[1].message, "f doesn't work with a value in Σ*: see row 1.");
    EXPECT_EQ(diagnostics[1].ref.Row(), 1u);
}

TEST(Check, RecursiveFunctions) {
    // The first case (or return) gives the type of the calls in later ones.
    EXPECT_EQ(FirstError("fact(n) \\{\nif n = 0 \\{ return 1 \\}\nreturn n \\cdot fact(n - 1)\n\\}\n"
                         "print(fact(5))"),
              "");
    EXPECT_EQ(FirstError("f(n) := f(n - 1)\nprint(f(1))"),
              "f uses itself before it gives a value, so write the type of its result, as in "
              "f(…) → ℤ := …");
    EXPECT_EQ(FirstError("f(n) -> \\mathbb{Z} := f(n - 1)\nprint(f(1))"), "");
}

TEST(Check, Cases) {
    EXPECT_EQ(FirstError("x := 1\ny := \\begin{cases}1 & x > 0\\\\2.5 & otherwise\\end{cases}"
                         "\nprint(y)"),
              "");
    EXPECT_EQ(FirstError("x := 1\ny := \\begin{cases}1 & otherwise\\\\2 & x > 0\\end{cases}"),
              "'otherwise' must be the last case.");
    EXPECT_EQ(FirstError("x := 1\ny := \\begin{cases}1 & x > 0\\\\\"a\" & otherwise"
                         "\\end{cases}"),
              "This case gives Σ*, but the ones before give ℤ.");
    EXPECT_EQ(FirstError("x := 1\ny := \\begin{cases}1 & x\\\\2 & otherwise\\end{cases}"),
              "The condition after a case must be true or false (𝔹), but this is in ℤ.");
}

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
