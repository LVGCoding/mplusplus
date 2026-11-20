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
        for (;;) {
            while (Peek().kind == Kind::Newline || Peek().IsOp(";")) {
                Advance();
            }
            if (Peek().kind == Kind::End) {
                break;
            }
            try {
                auto statement = ParseStatement();
                const Token& next = Peek();
                if (next.kind != Kind::Newline && next.kind != Kind::End && !next.IsOp(";")) {
                    Unexpected(next, "Expected the end of the statement");
                }
                program.statements.push_back(std::move(statement));
            } catch (const SyntaxError&) {
                // Skip the rest of the statement.
                depth_ = 0;
                while (Peek().kind != Kind::Newline && Peek().kind != Kind::End &&
                       !Peek().IsOp(";")) {
                    Advance();
                }
            }
        }
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

    const Token& PeekAfter() const {
        return pos_ + 1 < tokens_.size() ? tokens_[pos_ + 1] : end_;
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

    // --- Statements -------------------------------------------------------------

    std::unique_ptr<Stmt> ParseStatement() {
        const Token& first = Peek();
        if (first.kind == Kind::Word && IsKeyword(first.text) && first.text != "true" &&
            first.text != "false") {
            Fail("'" + first.text + "' can't be used yet: control flow is coming in the next "
                 "version of M++.", first.ref);
        }
        const Token& second = PeekAfter();
        if (first.kind == Kind::Word && !IsKeyword(first.text) &&
            (second.IsOp(":=") || second.IsOp("∈") || second.IsOp("←"))) {
            return ParseDefineOrAssign();
        }

        auto statement = std::make_unique<Stmt>();
        statement->kind = Stmt::Kind::Expr;
        statement->value = ParseExpression();
        statement->ref = statement->value->ref;
        if (Peek().IsOp(":=") || Peek().IsOp("←")) {
            const bool call = statement->value->kind == Expr::Kind::Call;
            Fail(call ? "Functions can't be defined yet: they're coming in the next version of M++."
                      : "Only a name can go on the left of " + Peek().text + ".",
                 statement->value->ref);
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

        const Token& op = Advance();
        if (op.IsOp("←")) {
            statement->kind = Stmt::Kind::Assign;
            statement->value = ParseExpression();
        } else {
            statement->kind = Stmt::Kind::Define;
            if (op.IsOp("∈")) {
                const Token& type = Peek();
                if (!IsBuiltinSet(type)) {
                    Unexpected(type, "Expected a type such as ℝ or ℤ after ∈");
                }
                Advance();
                statement->declaredType = type.text;
                statement->typeRef = type.ref;
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

    std::unique_ptr<Expr> ParseComparison() {
        auto first = ParseAdditive();
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
            expr->args.push_back(ParseAdditive());
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
                return !(token.shape == Shape::Power && token.slots[0].empty());
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
                next.IsOp("⊙")) {
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

    std::unique_ptr<Expr> ParsePostfix() {
        auto expr = ParsePrimary();
        for (;;) {
            const Token& next = Peek();
            if (next.kind == Kind::Structure && next.shape == Shape::Power &&
                next.slots[0].empty()) {
                const Token& power = Advance();
                auto exponent = ParseSlot(power.slots[1], power, "exponent");
                const SourceRef ref = Span(expr->ref, power.ref);
                expr = Power(std::move(expr), std::move(exponent), ref);
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
                auto exponent = ParseSlot(token.slots[1], token, "exponent");
                return Power(std::move(base), std::move(exponent), token.ref);
            }
            case Shape::Group: {
                auto inner = ParseSlot(token.slots[0], token, "group");
                inner->ref = token.ref;
                return inner;
            }
            case Shape::Matrix:
                Fail("Matrices can't be run yet: they're coming in a later version of M++.",
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
