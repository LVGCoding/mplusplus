#include "Parser.h"

#include "Builtins.h"

#include <array>
#include <utility>

namespace lang {

bool IsKeyword(std::string_view word) {
    static constexpr std::array<std::string_view, 11> KEYWORDS = {
        "if", "else", "while", "for", "return", "break", "continue", "in", "mod", "true", "false",
    };
    for (std::string_view keyword : KEYWORDS) {
        if (word == keyword) {
            return true;
        }
    }
    return false;
}

namespace {

using Kind = Token::Kind;
using Shape = Token::Shape;

// Thrown to abandon the statement being parsed after reporting its error.
struct SyntaxError {};

std::string Describe(const Token& token) {
    switch (token.kind) {
        case Kind::Number: return "the number " + token.text;
        case Kind::Word: return "'" + token.text + "'";
        case Kind::String: return "a string";
        case Kind::Op: return "'" + token.text + "'";
        case Kind::Newline: return "the end of the row";
        case Kind::End: return "the end";
        case Kind::Structure:
            switch (token.shape) {
                case Shape::Fraction: return "a fraction";
                case Shape::Root: return "a root";
                case Shape::Power: return "a power";
                case Shape::Matrix: return "a matrix";
                case Shape::Subscript: return "a subscript";
                case Shape::Graph: return "a graph";
                case Shape::Table: return "a table";
                default: return "a group";
            }
    }
    return "this";
}

bool IsBuiltinSet(const Token& token) {
    if (token.kind != Kind::Op) return false;
    for (std::string_view set : {"ℕ", "ℤ", "ℚ", "ℝ", "ℂ", "𝔹", "Σ"}) {
        if (token.text == set) return true;
    }
    return false;
}

std::unique_ptr<Expr> MakeExpr(Expr::Kind kind, SourceRef ref) {
    auto expr = std::make_unique<Expr>();
    expr->kind = kind;
    expr->ref = std::move(ref);
    return expr;
}

class SyntaxParser {
public:
    // `end` is where an unexpected end points: the structure a slot belongs
    // to, or the end of the document.
    SyntaxParser(const std::vector<Token>& tokens, std::vector<Diagnostic>& diagnostics,
                 SourceRef end)
        : tokens_(tokens), diagnostics_(diagnostics) {
        end_.kind = Kind::End;
        end_.ref = std::move(end);
    }

    Program ParseProgram() {
        Program program;
        program.statements = ParseStatements(nullptr);
        return program;
    }

    // A whole slot of a structure as one expression.
    std::unique_ptr<Expr> ParseWholeExpression(const char* what) {
        auto expr = ParseExpression();
        if (Peek().kind != Kind::End) {
            Unexpected(Peek(), std::string("Expected the end of the ") + what);
        }
        return expr;
    }

    // A cell of a cases structure that holds a condition: "x > 0", "if x > 0",
    // or "otherwise" (null).
    std::unique_ptr<Expr> ParseCondition() {
        if (Peek().IsWord("otherwise") || Peek().IsWord("else")) {
            Advance();
            if (Peek().kind != Kind::End) {
                Unexpected(Peek(), "Expected nothing after 'otherwise'");
            }
            return nullptr;
        }
        if (Peek().IsWord("if")) {
            Advance();
        }
        return ParseWholeExpression("condition");
    }

private:
    const std::vector<Token>& tokens_;
    std::vector<Diagnostic>& diagnostics_;
    Token end_;
    std::size_t pos_ = 0;
    // Open parentheses: inside them, rows continue onto the next.
    int depth_ = 0;
    // The token consumed last, to decide whether two values may be multiplied
    // by writing them next to each other.
    const Token* last_ = nullptr;

    const Token& Peek() {
        if (depth_ > 0) {
            while (pos_ < tokens_.size() && tokens_[pos_].kind == Kind::Newline) {
                ++pos_;
            }
        }
        return pos_ < tokens_.size() ? tokens_[pos_] : end_;
    }

    const Token& PeekAt(std::size_t offset) const {
        return pos_ + offset < tokens_.size() ? tokens_[pos_ + offset] : end_;
    }

    const Token& Advance() {
        const Token& token = Peek();
        if (pos_ < tokens_.size()) {
            ++pos_;
        }
        last_ = &token;
        return token;
    }

    // After a binary operator the expression continues on the next row.
    void SkipNewlines() {
        while (pos_ < tokens_.size() && tokens_[pos_].kind == Kind::Newline) {
            ++pos_;
        }
    }

    // The first token after any newlines, without consuming anything.
    const Token& PeekPastNewlines() const {
        std::size_t at = pos_;
        while (at < tokens_.size() && tokens_[at].kind == Kind::Newline) {
            ++at;
        }
        return at < tokens_.size() ? tokens_[at] : end_;
    }

    [[noreturn]] void Fail(std::string message, const SourceRef& ref) {
        diagnostics_.push_back({std::move(message), ref});
        throw SyntaxError{};
    }

    [[noreturn]] void Unexpected(const Token& token, const std::string& context) {
        Fail(context + ", but found " + Describe(token) + ".", token.ref);
    }

    const Token& Expect(std::string_view op, const char* context) {
        if (!Peek().IsOp(op)) {
            Unexpected(Peek(), context);
        }
        return Advance();
    }

    bool AtStatementEnd() {
        const Token& next = Peek();
        return next.kind == Kind::Newline || next.kind == Kind::End || next.IsOp(";") ||
               next.IsOp("}");
    }

    // --- Statements -------------------------------------------------------------

    // Statements up to the end, or up to the "}" closing the block opened by
    // `open` (which is left for the caller).
    Block ParseStatements(const Token* open) {
        Block statements;
        for (;;) {
            while (Peek().kind == Kind::Newline || Peek().IsOp(";")) {
                Advance();
            }
            const Token& next = Peek();
            if (next.kind == Kind::End) {
                if (open) {
                    diagnostics_.push_back(
                        {"This { is never closed: put a } at the end of the block.", open->ref});
                }
                return statements;
            }
            if (next.IsOp("}")) {
                if (open) {
                    return statements;
                }
                diagnostics_.push_back({"This } has no { before it to close.", next.ref});
                Advance();
                continue;
            }
            try {
                auto statement = ParseStatement();
                if (!AtStatementEnd()) {
                    Unexpected(Peek(), "Expected the end of the statement");
                }
                statements.push_back(std::move(statement));
            } catch (const SyntaxError&) {
                SkipStatement();
            }
        }
    }

    // After a syntax error: skip to the end of the statement, including any
    // block it opened, but not past the "}" of the block it is in.
    void SkipStatement() {
        depth_ = 0;
        int braces = 0;
        for (;;) {
            const Token& next = Peek();
            if (next.kind == Kind::End) {
                return;
            }
            if (braces == 0 &&
                (next.kind == Kind::Newline || next.IsOp(";") || next.IsOp("}"))) {
                return;
            }
            if (next.IsOp("{")) {
                ++braces;
            } else if (next.IsOp("}")) {
                --braces;
            }
            Advance();
        }
    }

    // { statements }, starting at the "{".
    Block ParseBlock(const char* after) {
        if (!Peek().IsOp("{")) {
            Unexpected(Peek(), std::string("Expected { to start the block after ") + after);
        }
        const Token& open = Advance();
        Block body = ParseStatements(&open);
        if (Peek().IsOp("}")) {
            Advance();
        }
        return body;
    }

    std::unique_ptr<Stmt> MakeStmt(Stmt::Kind kind, const SourceRef& ref) {
        auto statement = std::make_unique<Stmt>();
        statement->kind = kind;
        statement->ref = ref;
        return statement;
    }

    std::unique_ptr<Stmt> ParseStatement() {
        const Token& first = Peek();
        if (first.kind == Kind::Word) {
            if (first.text == "if") return ParseIf();
            if (first.text == "while") return ParseWhile();
            if (first.text == "for") return ParseFor();
            if (first.text == "break" || first.text == "continue") {
                Advance();
                return MakeStmt(first.text == "break" ? Stmt::Kind::Break : Stmt::Kind::Continue,
                                first.ref);
            }
            if (first.text == "return") {
                Advance();
                auto statement = MakeStmt(Stmt::Kind::Return, first.ref);
                if (!AtStatementEnd()) {
                    statement->value = ParseExpression();
                    statement->ref = Span(first.ref, statement->value->ref);
                }
                return statement;
            }
            if (first.text == "else") {
                Fail("else goes right after the } of an if.", first.ref);
            }
        }
        const Token& second = PeekAt(1);
        auto definesOrAssigns = [](const Token& op) {
            return op.IsOp(":=") || op.IsOp("∈") || op.IsOp("←");
        };
        if (first.kind == Kind::Word && !IsKeyword(first.text) &&
            (definesOrAssigns(second) ||
             (second.kind == Kind::Structure && second.shape == Shape::Subscript &&
              definesOrAssigns(PeekAt(2))))) {
            return ParseDefineOrAssign();
        }
        if (first.kind == Kind::Word && !IsKeyword(first.text) && second.IsOp("(") &&
            IsFunctionDefinition()) {
            return ParseFunction();
        }
        if (first.IsOp("{")) {
            Fail("A block ({ … }) goes after if, while, for or a function.", first.ref);
        }

        auto statement = MakeStmt(Stmt::Kind::Expr, {});
        statement->value = ParseExpression();
        statement->ref = statement->value->ref;
        if (Peek().IsOp(":=") || Peek().IsOp("←")) {
            Fail("Only a name can go on the left of " + Peek().text + ".", statement->value->ref);
        }
        return statement;
    }

    std::unique_ptr<Stmt> ParseIf() {
        const Token& keyword = Advance();
        auto statement = MakeStmt(Stmt::Kind::If, keyword.ref);
        statement->value = ParseExpression();
        statement->ref = Span(keyword.ref, statement->value->ref);
        statement->body = ParseBlock("the condition");
        // "else" may start the next row: } \n else {
        if (PeekPastNewlines().IsWord("else")) {
            SkipNewlines();
            const Token& elseWord = Advance();
            statement->hasElse = true;
            if (Peek().IsWord("if")) {
                statement->elseBody.push_back(ParseIf());
            } else {
                if (!Peek().IsOp("{")) {
                    Unexpected(Peek(), "Expected { or 'if' after else");
                }
                statement->elseBody = ParseBlock("else");
            }
            (void)elseWord;
        }
        return statement;
    }

    std::unique_ptr<Stmt> ParseWhile() {
        const Token& keyword = Advance();
        auto statement = MakeStmt(Stmt::Kind::While, keyword.ref);
        statement->value = ParseExpression();
        statement->ref = Span(keyword.ref, statement->value->ref);
        statement->body = ParseBlock("the condition");
        return statement;
    }

    std::unique_ptr<Stmt> ParseFor() {
        const Token& keyword = Advance();
        auto statement = MakeStmt(Stmt::Kind::For, keyword.ref);
        const Token& name = Peek();
        if (name.kind != Kind::Word || IsKeyword(name.text)) {
            Unexpected(name, "Expected the name of the loop variable after for");
        }
        Advance();
        statement->name = name.text;
        statement->ident = name.ident;
        statement->greek = name.greek;
        statement->nameRef = name.ref;
        if (!Peek().IsOp("∈") && !Peek().IsWord("in")) {
            Unexpected(Peek(), "Expected ∈ after the loop variable, as in for i ∈ 1..n");
        }
        Advance();
        statement->value = ParseExpression();
        if (statement->value->kind != Expr::Kind::Range) {
            Fail("for needs a range, such as 1..n.", statement->value->ref);
        }
        statement->ref = Span(keyword.ref, statement->value->ref);
        statement->body = ParseBlock("the range");
        return statement;
    }

    // At "name(": is this f(…) := …, f(…) → T …, or f(…) { … }? Looks past
    // the matching ")" without consuming anything.
    bool IsFunctionDefinition() const {
        int depth = 0;
        for (std::size_t at = pos_ + 1; at < tokens_.size(); ++at) {
            const Token& token = tokens_[at];
            if (token.IsOp("(")) {
                ++depth;
            } else if (token.IsOp(")")) {
                if (--depth == 0) {
                    const Token& after = at + 1 < tokens_.size() ? tokens_[at + 1] : end_;
                    return after.IsOp(":=") || after.IsOp("→") || after.IsOp("{");
                }
            } else if (token.kind == Kind::End) {
                return false;
            }
        }
        return false;
    }

    struct TypeSpec {
        std::string set;
        SourceRef ref;
        std::vector<std::shared_ptr<Expr>> dims;
    };

    // A type after ∈ or →: a set such as ℝ, or ℝ^n, ℝ^{n×m}, ℝ^*.
    TypeSpec ParseType(const char* context) {
        const Token& token = Peek();
        TypeSpec type;
        if (IsBuiltinSet(token)) {
            Advance();
            type.set = token.text;
            type.ref = token.ref;
            // ℝ followed by a power typed after it.
            const Token& next = Peek();
            if (next.kind == Kind::Structure && next.shape == Shape::Power &&
                next.slots[0].empty()) {
                const Token& power = Advance();
                type.dims = ParseDims(power.slots[1], power);
                type.ref = Span(token.ref, power.ref);
            }
            return type;
        }
        // A power whose base is the set: \exp{ℝ}{n}.
        if (token.kind == Kind::Structure && token.shape == Shape::Power &&
            token.slots[0].size() == 1 && IsBuiltinSet(token.slots[0][0])) {
            Advance();
            type.set = token.slots[0][0].text;
            type.ref = token.ref;
            type.dims = ParseDims(token.slots[1], token);
            return type;
        }
        Unexpected(token, context);
    }

    // The sizes in the exponent of ℝ^…: n, n×m, or * for any length.
    std::vector<std::shared_ptr<Expr>> ParseDims(const std::vector<Token>& slot,
                                                 const Token& owner) {
        std::vector<std::shared_ptr<Expr>> dims;
        if (slot.size() == 1 && slot[0].IsOp("⋅")) {
            dims.push_back(nullptr);
            return dims;
        }
        auto size = ParseSlot(slot, owner, "size");
        if (size->kind == Expr::Kind::Binary && size->op == "×") {
            dims.push_back(std::move(size->args[0]));
            dims.push_back(std::move(size->args[1]));
        } else {
            dims.push_back(std::move(size));
        }
        return dims;
    }

    std::unique_ptr<Stmt> ParseFunction() {
        const Token& name = Advance();
        auto statement = MakeStmt(Stmt::Kind::Function, name.ref);
        statement->name = name.text;
        statement->ident = name.ident;
        statement->greek = name.greek;
        statement->nameRef = name.ref;
        Advance();  // (
        ++depth_;
        if (!Peek().IsOp(")")) {
            for (;;) {
                const Token& parameter = Peek();
                if (parameter.kind != Kind::Word || IsKeyword(parameter.text)) {
                    Unexpected(parameter, "Expected the name of a parameter");
                }
                Advance();
                Param param{.name = parameter.text, .ident = parameter.ident,
                            .greek = parameter.greek, .ref = parameter.ref};
                if (Peek().IsOp("∈")) {
                    Advance();
                    TypeSpec type = ParseType("Expected a type such as ℝ or ℤ after ∈");
                    param.declaredType = type.set;
                    param.typeRef = type.ref;
                    param.dims = std::move(type.dims);
                }
                statement->params.push_back(std::move(param));
                if (!Peek().IsOp(",")) {
                    break;
                }
                Advance();
            }
        }
        const Token& close = Expect(")", "Expected ',' or ')' after the parameters");
        --depth_;
        statement->ref = Span(name.ref, close.ref);
        if (Peek().IsOp("→")) {
            Advance();
            TypeSpec type = ParseType("Expected the type of the result after →, such as ℝ");
            statement->declaredType = type.set;
            statement->typeRef = type.ref;
            statement->dims = std::move(type.dims);
        }
        if (Peek().IsOp(":=")) {
            const Token& define = Advance();
            if (Peek().IsOp("{")) {
                Fail("For a function written over several rows, leave out :=, as in " +
                         name.text + "(…) { … }",
                     define.ref);
            }
            statement->value = ParseExpression();
            statement->ref = Span(name.ref, statement->value->ref);
        } else {
            statement->body = ParseBlock("the parameters");
        }
        return statement;
    }

    std::unique_ptr<Stmt> ParseDefineOrAssign() {
        auto statement = std::make_unique<Stmt>();
        const Token& name = Advance();
        statement->name = name.text;
        statement->ident = name.ident;
        statement->greek = name.greek;
        statement->nameRef = name.ref;
        if (Peek().kind == Kind::Structure && Peek().shape == Shape::Subscript) {
            const Token& subscript = Advance();
            statement->indices = ParseSubscript(subscript);
            statement->indexRef = subscript.ref;
        }

        const Token& op = Advance();
        if (op.IsOp("←")) {
            statement->kind = Stmt::Kind::Assign;
            statement->value = ParseExpression();
        } else {
            statement->kind = Stmt::Kind::Define;
            if (op.IsOp("∈")) {
                TypeSpec type = ParseType("Expected a type such as ℝ or ℤ after ∈");
                statement->declaredType = type.set;
                statement->typeRef = type.ref;
                statement->dims = std::move(type.dims);
                if (Peek().IsOp(":=")) {
                    Advance();
                    statement->value = ParseExpression();
                }
            } else {
                statement->value = ParseExpression();
            }
        }
        statement->ref = Span(name.ref, statement->value ? statement->value->ref
                                        : statement->typeRef);
        return statement;
    }

    // The items of a subscript, separated by commas; * alone is All.
    std::vector<std::unique_ptr<Expr>> ParseSubscript(const Token& subscript) {
        const std::vector<Token>& slot = subscript.slots[0];
        if (slot.empty()) {
            Fail("The subscript is empty.", subscript.ref);
        }
        SyntaxParser parser(slot, diagnostics_, subscript.ref);
        return parser.ParseSubscriptItems();
    }

public:
    std::vector<std::unique_ptr<Expr>> ParseSubscriptItems() {
        std::vector<std::unique_ptr<Expr>> items;
        for (;;) {
            if (Peek().IsOp("⋅") && (PeekAt(1).IsOp(",") || PeekAt(1).kind == Kind::End)) {
                items.push_back(MakeExpr(Expr::Kind::All, Advance().ref));
            } else {
                items.push_back(ParseExpression());
            }
            if (Peek().kind == Kind::End) {
                return items;
            }
            Expect(",", "Expected ',' or the end of the subscript");
        }
    }

private:

    // --- Expressions, lowest precedence first ------------------------------------

    std::unique_ptr<Expr> ParseExpression() { return ParseOr(); }

    std::unique_ptr<Expr> Binary(std::string op, const SourceRef& opRef,
                                 std::unique_ptr<Expr> left, std::unique_ptr<Expr> right) {
        auto expr = MakeExpr(Expr::Kind::Binary, Span(left->ref, right->ref));
        expr->op = std::move(op);
        expr->opRef = opRef;
        expr->args.push_back(std::move(left));
        expr->args.push_back(std::move(right));
        return expr;
    }

    std::unique_ptr<Expr> ParseOr() {
        auto left = ParseAnd();
        while (Peek().IsOp("∨") || Peek().IsOp("⊕")) {
            const Token& op = Advance();
            SkipNewlines();
            left = Binary(op.text, op.ref, std::move(left), ParseAnd());
        }
        return left;
    }

    std::unique_ptr<Expr> ParseAnd() {
        auto left = ParseNot();
        while (Peek().IsOp("∧")) {
            const Token& op = Advance();
            SkipNewlines();
            left = Binary(op.text, op.ref, std::move(left), ParseNot());
        }
        return left;
    }

    std::unique_ptr<Expr> ParseNot() {
        if (Peek().IsOp("¬")) {
            const Token& op = Advance();
            auto operand = ParseNot();
            auto expr = MakeExpr(Expr::Kind::Unary, Span(op.ref, operand->ref));
            expr->op = op.text;
            expr->opRef = op.ref;
            expr->args.push_back(std::move(operand));
            return expr;
        }
        return ParseComparison();
    }

    static bool IsComparison(const Token& token) {
        if (token.kind != Kind::Op) return false;
        for (std::string_view op : {"=", "≠", "<", "≤", ">", "≥", "≈"}) {
            if (token.text == op) return true;
        }
        return false;
    }

    // a..b, or a..s..b with a step.
    std::unique_ptr<Expr> ParseRange() {
        auto first = ParseAdditive();
        if (!Peek().IsOp("..")) {
            return first;
        }
        auto range = MakeExpr(Expr::Kind::Range, first->ref);
        range->opRef = Advance().ref;
        range->args.push_back(std::move(first));
        range->args.push_back(ParseAdditive());
        if (Peek().IsOp("..")) {
            Advance();
            range->args.push_back(ParseAdditive());
        }
        range->ref = Span(range->args.front()->ref, range->args.back()->ref);
        return range;
    }

    std::unique_ptr<Expr> ParseComparison() {
        auto first = ParseRange();
        if (!IsComparison(Peek())) {
            if (Peek().IsOp("∈") || Peek().IsOp("∉")) {
                Fail("Sets can't be used yet: they're coming in a later version of M++.",
                     Peek().ref);
            }
            return first;
        }
        auto expr = MakeExpr(Expr::Kind::Compare, first->ref);
        expr->args.push_back(std::move(first));
        while (IsComparison(Peek())) {
            const Token& op = Advance();
            SkipNewlines();
            expr->ops.push_back(op.text);
            if (expr->ops.size() == 1) {
                expr->opRef = op.ref;
            }
            expr->args.push_back(ParseRange());
        }
        expr->ref = Span(expr->args.front()->ref, expr->args.back()->ref);
        return expr;
    }

    std::unique_ptr<Expr> ParseAdditive() {
        auto left = ParseMultiplicative();
        while (Peek().IsOp("+") || Peek().IsOp("−")) {
            const Token& op = Advance();
            SkipNewlines();
            left = Binary(op.text, op.ref, std::move(left), ParseMultiplicative());
        }
        return left;
    }

    static bool StartsOperand(const Token& token) {
        switch (token.kind) {
            case Kind::Number:
            case Kind::String:
                return true;
            case Kind::Word:
                return token.text != "mod";
            case Kind::Op:
                return token.text == "(" || token.text == "∞";
            case Kind::Structure:
                return !(token.shape == Shape::Power && token.slots[0].empty()) &&
                       token.shape != Shape::Subscript;
            default:
                return false;
        }
    }

    // Writing two values next to each other multiplies them only where it
    // can't mean anything else: after a number (2x), next to a structure
    // (x√2, ½x), or after a closing parenthesis ((a+b)(a−b)).
    void CheckJuxtaposition(const Token& before, const Token& after) {
        if (before.kind == Kind::Number && after.kind != Kind::Number &&
            after.kind != Kind::String) {
            return;
        }
        if (before.kind == Kind::Structure || after.kind == Kind::Structure) {
            return;
        }
        if (before.IsOp(")") && (after.IsOp("(") || after.kind == Kind::Word)) {
            return;
        }
        const SourceRef both = Span(before.ref, after.ref);
        if (before.kind == Kind::Word && after.kind == Kind::Word) {
            if (FindBuiltinFunction(before.text)) {
                Fail("Write " + before.text + "(" + after.text + ") to use the function " +
                         before.text + ".", both);
            }
            Fail("Write " + before.text + "⋅" + after.text + " to multiply " + before.text +
                     " and " + after.text + ".", both);
        }
        if (before.kind == Kind::String || after.kind == Kind::String) {
            Fail("Is a comma missing here?", both);
        }
        Fail("Write ⋅ between these to multiply them.", both);
    }

    std::unique_ptr<Expr> ParseMultiplicative() {
        auto left = ParseUnary();
        for (;;) {
            const Token& next = Peek();
            if (next.IsOp("⋅") || next.IsOp("×") || next.IsOp("/") || next.IsWord("mod") ||
                next.IsOp("⊙") || next.IsOp("∖")) {
                const Token& op = Advance();
                SkipNewlines();
                left = Binary(op.text, op.ref, std::move(left), ParseUnary());
            } else if (StartsOperand(next)) {
                CheckJuxtaposition(*last_, next);
                // Implicit multiplication: no operator token, so it points
                // at the space between the two values.
                SourceRef between{left->ref.end, left->ref.end};
                left = Binary("⋅", between, std::move(left), ParsePostfix());
            } else {
                return left;
            }
        }
    }

    std::unique_ptr<Expr> ParseUnary() {
        if (Peek().IsOp("−") || Peek().IsOp("+")) {
            const Token& op = Advance();
            auto operand = ParseUnary();
            auto expr = MakeExpr(Expr::Kind::Unary, Span(op.ref, operand->ref));
            expr->op = op.text;
            expr->opRef = op.ref;
            expr->args.push_back(std::move(operand));
            return expr;
        }
        return ParsePostfix();
    }

    std::unique_ptr<Expr> Power(std::unique_ptr<Expr> base, std::unique_ptr<Expr> exponent,
                                const SourceRef& ref) {
        auto expr = MakeExpr(Expr::Kind::Power, ref);
        expr->args.push_back(std::move(base));
        expr->args.push_back(std::move(exponent));
        return expr;
    }

    // base^{exponent}, or base^⊤ (the transpose).
    std::unique_ptr<Expr> PowerOrTranspose(std::unique_ptr<Expr> base, const Token& power,
                                           const SourceRef& ref) {
        const std::vector<Token>& exponent = power.slots[1];
        if (exponent.size() == 1 && exponent[0].IsOp("⊤")) {
            auto transpose = MakeExpr(Expr::Kind::Transpose, ref);
            transpose->opRef = exponent[0].ref;
            transpose->args.push_back(std::move(base));
            return transpose;
        }
        return Power(std::move(base), ParseSlot(exponent, power, "exponent"), ref);
    }

    std::unique_ptr<Expr> ParsePostfix() {
        auto expr = ParsePrimary();
        for (;;) {
            const Token& next = Peek();
            if (next.kind == Kind::Structure && next.shape == Shape::Power &&
                next.slots[0].empty()) {
                const Token& power = Advance();
                const SourceRef ref = Span(expr->ref, power.ref);
                expr = PowerOrTranspose(std::move(expr), power, ref);
            } else if (next.kind == Kind::Structure && next.shape == Shape::Subscript) {
                const Token& subscript = Advance();
                auto index = MakeExpr(Expr::Kind::Index, Span(expr->ref, subscript.ref));
                index->opRef = subscript.ref;
                index->args.push_back(std::move(expr));
                for (auto& item : ParseSubscript(subscript)) {
                    index->args.push_back(std::move(item));
                }
                expr = std::move(index);
            } else if (next.IsOp("^")) {
                Advance();
                // Right-associative: 2^3^2 is 2^(3^2).
                std::unique_ptr<Expr> exponent;
                if (Peek().IsOp("−")) {
                    exponent = ParseUnary();
                } else {
                    exponent = ParsePostfix();
                }
                const SourceRef ref = Span(expr->ref, exponent->ref);
                expr = Power(std::move(expr), std::move(exponent), ref);
            } else if (next.IsOp("!")) {
                Fail("Factorials can't be used yet.", next.ref);
            } else {
                return expr;
            }
        }
    }

    // The contents of one slot of a structure token.
    std::unique_ptr<Expr> ParseSlot(const std::vector<Token>& slot, const Token& owner,
                                    const char* what) {
        if (slot.empty()) {
            Fail(std::string("The ") + what + " is empty.", owner.ref);
        }
        SyntaxParser parser(slot, diagnostics_, owner.ref);
        return parser.ParseWholeExpression(what);
    }

    std::unique_ptr<Expr> ParsePrimary() {
        const Token& token = Peek();
        switch (token.kind) {
            case Kind::Number: {
                Advance();
                auto expr = MakeExpr(Expr::Kind::Number, token.ref);
                expr->text = token.text;
                return expr;
            }
            case Kind::String: {
                Advance();
                auto expr = MakeExpr(Expr::Kind::String, token.ref);
                expr->text = token.text;
                return expr;
            }
            case Kind::Word: return ParseWord();
            case Kind::Structure: return ParseStructure();
            case Kind::Op:
                if (token.IsOp("(")) {
                    const Token& open = Advance();
                    ++depth_;
                    auto inner = ParseExpression();
                    const Token& close = Expect(")", "Expected ')' to close the '('");
                    --depth_;
                    inner->ref = Span(open.ref, close.ref);
                    return inner;
                }
                if (token.IsOp("|")) {
                    const Token& open = Advance();
                    ++depth_;
                    auto inner = ParseExpression();
                    const Token& close = Expect("|", "Expected '|' to close the '|'");
                    --depth_;
                    auto call = MakeExpr(Expr::Kind::Call, Span(open.ref, close.ref));
                    call->text = "abs";
                    call->opRef = open.ref;
                    call->args.push_back(std::move(inner));
                    return call;
                }
                if (token.IsOp("{")) {
                    Fail("A block ({ … }) goes after if, while, for or a function.", token.ref);
                }
                if (token.IsOp("∞")) {
                    Advance();
                    return MakeExpr(Expr::Kind::Infinity, token.ref);
                }
                if (IsBuiltinSet(token)) {
                    Fail(token.text + " is a type; it can only be used after ∈, as in x ∈ " +
                             token.text + " := …", token.ref);
                }
                break;
            default: break;
        }
        Unexpected(token, "Expected a value");
    }

    std::unique_ptr<Expr> ParseWord() {
        const Token& word = Advance();
        if (word.text == "true" || word.text == "false") {
            auto expr = MakeExpr(Expr::Kind::Bool, word.ref);
            expr->text = word.text;
            return expr;
        }
        if (IsKeyword(word.text)) {
            Fail("'" + word.text + "' can't be used here.", word.ref);
        }
        if (Peek().IsOp("(")) {
            return ParseCall(word);
        }
        auto expr = MakeExpr(Expr::Kind::Name, word.ref);
        expr->text = word.text;
        expr->ident = word.ident;
        expr->greek = word.greek;
        return expr;
    }

    std::unique_ptr<Expr> ParseCall(const Token& name) {
        Advance();  // (
        ++depth_;
        auto call = MakeExpr(Expr::Kind::Call, name.ref);
        call->text = name.text;
        call->opRef = name.ref;
        if (!Peek().IsOp(")")) {
            for (;;) {
                call->args.push_back(ParseExpression());
                if (!Peek().IsOp(",")) {
                    break;
                }
                Advance();
            }
        }
        const Token& close = Expect(")", "Expected ',' or ')' in the call");
        --depth_;
        call->ref = Span(name.ref, close.ref);
        return call;
    }

    // A matrix of numbers; a single column is a vector. A vmatrix (bars) is
    // its determinant.
    std::unique_ptr<Expr> ParseMatrix(const Token& token) {
        auto expr = MakeExpr(Expr::Kind::Matrix, token.ref);
        expr->text = token.text;
        expr->opRef = token.ref;
        expr->rows = token.rowLengths.size();
        expr->columns = token.rowLengths.empty() ? 0 : token.rowLengths.front();
        for (std::size_t length : token.rowLengths) {
            if (length != expr->columns) {
                Fail("Every row of a matrix needs the same number of entries.", token.ref);
            }
        }
        for (const auto& cell : token.slots) {
            expr->args.push_back(ParseSlot(cell, token, "matrix entry"));
        }
        return expr;
    }

    // Rows of "value & condition"; the last condition may be "otherwise".
    std::unique_ptr<Expr> ParseCases(const Token& token) {
        auto expr = MakeExpr(Expr::Kind::Cases, token.ref);
        expr->opRef = token.ref;
        std::size_t slot = 0;
        for (std::size_t length : token.rowLengths) {
            if (length != 2) {
                Fail("Each row of cases needs a value and a condition (or 'otherwise').",
                     token.ref);
            }
            expr->args.push_back(ParseSlot(token.slots[slot], token, "value"));
            const std::vector<Token>& condition = token.slots[slot + 1];
            if (condition.empty()) {
                Fail("A case needs a condition, or 'otherwise'.", token.ref);
            }
            SyntaxParser parser(condition, diagnostics_, token.ref);
            expr->conditions.push_back(parser.ParseCondition());
            slot += length;
        }
        return expr;
    }

    std::unique_ptr<Expr> ParseStructure() {
        const Token& token = Advance();
        switch (token.shape) {
            case Shape::Fraction: {
                auto expr = MakeExpr(Expr::Kind::Fraction, token.ref);
                expr->opRef = token.ref;
                expr->args.push_back(ParseSlot(token.slots[0], token, "numerator"));
                expr->args.push_back(ParseSlot(token.slots[1], token, "denominator"));
                return expr;
            }
            case Shape::Root: {
                auto expr = MakeExpr(Expr::Kind::Root, token.ref);
                expr->opRef = token.ref;
                expr->args.push_back(ParseSlot(token.slots[0], token, "root"));
                if (token.slots.size() > 1) {
                    expr->args.push_back(ParseSlot(token.slots[1], token, "root's index"));
                }
                return expr;
            }
            case Shape::Power: {
                if (token.slots[0].empty()) {
                    Fail("This power has nothing to raise: put something before it, or in its "
                         "first box.", token.ref);
                }
                auto base = ParseSlot(token.slots[0], token, "base");
                return PowerOrTranspose(std::move(base), token, token.ref);
            }
            case Shape::Group: {
                auto inner = ParseSlot(token.slots[0], token, "group");
                inner->ref = token.ref;
                return inner;
            }
            case Shape::Delimited: {
                auto call = MakeExpr(Expr::Kind::Call, token.ref);
                call->text = token.text;
                call->opRef = token.ref;
                call->args.push_back(ParseSlot(token.slots[0], token,
                                               token.text == "abs" ? "absolute value"
                                                                   : "brackets"));
                return call;
            }
            case Shape::Cases: return ParseCases(token);
            case Shape::Matrix: return ParseMatrix(token);
            case Shape::Subscript:
                Fail("This subscript has nothing before it: put a name in front, as in x_i.",
                     token.ref);
            case Shape::Graph:
            case Shape::Table:
                Fail("Graphs and tables can't be run yet: they're coming in a later version "
                     "of M++.", token.ref);
            case Shape::None: break;
        }
        Unexpected(token, "Expected a value");
    }
};

}  // namespace

Program Parse(const std::vector<Token>& tokens, std::vector<Diagnostic>& diagnostics) {
    SyntaxParser parser(tokens, diagnostics, tokens.empty() ? SourceRef{} : tokens.back().ref);
    return parser.ParseProgram();
}

}  // namespace lang
