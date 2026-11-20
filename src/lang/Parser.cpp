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
                case Shape::BigOp: return "a " + token.text;
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
        if (first.kind == Kind::Word && !IsKeyword(first.text) && second.IsOp(".") &&
            PeekAt(2).kind == Kind::Word && (PeekAt(3).IsOp("←") || PeekAt(3).IsOp(":="))) {
            return ParseFieldAssign();
        }
        if (first.kind == Kind::Word && !IsKeyword(first.text) && second.IsOp("(") &&
            IsFunctionDefinition()) {
            return ParseFunction();
        }
        if (first.IsOp("{") && PeekAt(1).kind == Kind::Newline) {
            Fail("A block ({ … }) goes after if, while, for or a function.", first.ref);
        }

        auto statement = MakeStmt(Stmt::Kind::Expr, {});
        statement->value = ParseExpression();
        statement->ref = statement->value->ref;
        Expr& target = *statement->value;
        if ((Peek().IsOp(":=") || Peek().IsOp("←")) && target.kind == Expr::Kind::Index &&
            target.args.size() == 2 && target.args[0]->kind == Expr::Kind::Name) {
            // m["key"] ← value: the same as m_{"key"} ← value.
            const Token& op = Advance();
            Expr& name = *target.args[0];
            statement->kind = op.IsOp("←") ? Stmt::Kind::Assign : Stmt::Kind::Define;
            statement->name = name.text;
            statement->ident = name.ident;
            statement->greek = name.greek;
            statement->nameRef = name.ref;
            statement->indexRef = target.opRef;
            statement->indices.push_back(std::move(target.args[1]));
            // (This replaces `target`, so `name` can't be used after it.)
            statement->value = ParseExpression();
            statement->ref = Span(statement->nameRef, statement->value->ref);
            return statement;
        }
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
        // A range (1..n), a set or a vector; not a comparison, so the block
        // that follows isn't taken for part of it.
        statement->value = ParseRange();
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
        std::string mapKey;
        bool list = false;
        std::shared_ptr<Expr> choices;

        template <class Target>
        void ApplyTo(Target& target) {
            target.declaredType = std::move(set);
            target.typeRef = ref;
            target.dims = std::move(dims);
            target.mapKey = std::move(mapKey);
            target.list = list;
            target.choices = std::move(choices);
        }
    };

    // Σ* written as Σ followed by *, at `slot[at]`; returns the set's name.
    static std::string SetName(const std::vector<Token>& tokens, std::size_t at) {
        if (tokens[at].IsOp("Σ") && at + 1 < tokens.size() && tokens[at + 1].IsOp("⋅")) {
            return "Σ*";
        }
        return tokens[at].text;
    }

    // The exponent of ℝ^…: sizes (n, n×m, *) or, if it is a set, the keys
    // of a map (ℝ^{Σ*}).
    void ParseExponentOfType(TypeSpec& type, const std::vector<Token>& slot, const Token& owner) {
        if (!slot.empty() && IsBuiltinSet(slot[0])) {
            type.mapKey = SetName(slot, 0);
            const std::size_t used = type.mapKey == "Σ*" ? 2 : 1;
            if (slot.size() != used) {
                Fail("A map's keys are written as one set, as in ℝ^{Σ*}.", owner.ref);
            }
            return;
        }
        type.dims = ParseDims(slot, owner);
    }

    // A type after ∈ or →: a set such as ℝ or Σ*; ℝ^n, ℝ^{n×m}, ℝ^*;
    // a map ℝ^{Σ*}; a struct type's name; choices {"a", "b"}; any of these
    // followed by * for a list.
    TypeSpec ParseType(const char* context) {
        const Token& token = Peek();
        TypeSpec type;
        if (IsBuiltinSet(token)) {
            Advance();
            type.set = token.text;
            type.ref = token.ref;
            if (token.IsOp("Σ") && Peek().IsOp("⋅")) {
                type.set = "Σ*";
                type.ref = Span(token.ref, Advance().ref);
            }
            // ℝ followed by a power typed after it.
            const Token& next = Peek();
            if (next.kind == Kind::Structure && next.shape == Shape::Power &&
                next.slots[0].empty()) {
                const Token& power = Advance();
                ParseExponentOfType(type, power.slots[1], power);
                type.ref = Span(token.ref, power.ref);
            }
        } else if (token.kind == Kind::Structure && token.shape == Shape::Power &&
                   !token.slots[0].empty() && IsBuiltinSet(token.slots[0][0])) {
            // A power whose base is the set: \exp{ℝ}{n}.
            Advance();
            type.set = SetName(token.slots[0], 0);
            type.ref = token.ref;
            ParseExponentOfType(type, token.slots[1], token);
        } else if (token.kind == Kind::Word && !IsKeyword(token.text)) {
            Advance();
            type.set = token.text;  // a struct type, found by the checker
            type.ref = token.ref;
        } else if (token.IsOp("{")) {
            type.choices = ParseSet();
            type.ref = type.choices->ref;
            return type;
        } else {
            Unexpected(token, context);
        }
        if (Peek().IsOp("⋅")) {
            type.list = true;
            type.ref = Span(type.ref, Advance().ref);
        }
        return type;
    }

    // @label("Name") @align("right") …: tags after a struct field's type.
    std::vector<Tag> ParseTags() {
        std::vector<Tag> tags;
        while (Peek().IsOp("@")) {
            const Token& at = Advance();
            const Token& name = Peek();
            if (name.kind != Kind::Word) {
                Unexpected(name, "Expected the name of a tag after @, as in @label(\"Name\")");
            }
            Advance();
            Tag tag{.name = name.text, .ref = Span(at.ref, name.ref)};
            if (Peek().IsOp("(")) {
                Advance();
                while (!Peek().IsOp(")")) {
                    const Token& value = Peek();
                    if (value.kind == Kind::String || value.kind == Kind::Number) {
                        tag.args.push_back(value.text);
                        Advance();
                    } else if (value.IsOp("−") && PeekAt(1).kind == Kind::Number) {
                        Advance();
                        tag.args.push_back("-" + Advance().text);
                    } else if (value.kind == Kind::Word) {
                        tag.args.push_back(value.text);
                        Advance();
                    } else {
                        Unexpected(value, "Expected a string or number in the tag");
                    }
                    if (Peek().IsOp(",")) {
                        Advance();
                    } else if (!Peek().IsOp(")")) {
                        Unexpected(Peek(), "Expected ',' or ')' in the tag");
                    }
                }
                tag.ref = Span(tag.ref, Advance().ref);
            }
            tags.push_back(std::move(tag));
        }
        return tags;
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
                    ParseType("Expected a type such as ℝ or ℤ after ∈").ApplyTo(param);
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
            ParseType("Expected the type of the result after →, such as ℝ").ApplyTo(*statement);
        }
        if (Peek().IsOp(":=")) {
            const Token& define = Advance();
            if (Peek().IsOp("{") && PeekAt(1).kind == Kind::Newline) {
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
                ParseType("Expected a type such as ℝ or ℤ after ∈").ApplyTo(*statement);
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

    // p.name ← value
    std::unique_ptr<Stmt> ParseFieldAssign() {
        auto statement = MakeStmt(Stmt::Kind::Assign, {});
        const Token& name = Advance();
        statement->name = name.text;
        statement->ident = name.ident;
        statement->greek = name.greek;
        statement->nameRef = name.ref;
        Advance();  // .
        const Token& member = Advance();
        statement->member = member.text;
        statement->memberRef = member.ref;
        const Token& op = Advance();
        if (!op.IsOp("←")) {
            Fail("To change a field, write " + name.text + "." + member.text + " ← …", op.ref);
        }
        statement->value = ParseExpression();
        statement->ref = Span(name.ref, statement->value->ref);
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
        if (token.IsWord("in")) return true;  // "lo" in "hello": a substring
        if (token.kind != Kind::Op) return false;
        for (std::string_view op : {"=", "≠", "<", "≤", ">", "≥", "≈", "∈", "∉", "⊂", "⊆"}) {
            if (token.text == op) return true;
        }
        return false;
    }

    // a..b, or a..s..b with a step.
    std::unique_ptr<Expr> ParseRange() {
        auto first = ParseSetUnion();
        if (!Peek().IsOp("..")) {
            return first;
        }
        auto range = MakeExpr(Expr::Kind::Range, first->ref);
        range->opRef = Advance().ref;
        range->args.push_back(std::move(first));
        range->args.push_back(ParseSetUnion());
        if (Peek().IsOp("..")) {
            Advance();
            range->args.push_back(ParseSetUnion());
        }
        range->ref = Span(range->args.front()->ref, range->args.back()->ref);
        return range;
    }

    // A ∪ B, A ∖ B, A Δ B (A∖b also solves A⋅x = b for a matrix A).
    std::unique_ptr<Expr> ParseSetUnion() {
        auto left = ParseSetIntersection();
        while (Peek().IsOp("∪") || Peek().IsOp("∖") || Peek().IsOp("Δ")) {
            const Token& op = Advance();
            SkipNewlines();
            left = Binary(op.text, op.ref, std::move(left), ParseSetIntersection());
        }
        return left;
    }

    std::unique_ptr<Expr> ParseSetIntersection() {
        auto left = ParseAdditive();
        while (Peek().IsOp("∩")) {
            const Token& op = Advance();
            SkipNewlines();
            left = Binary(op.text, op.ref, std::move(left), ParseAdditive());
        }
        return left;
    }

    std::unique_ptr<Expr> ParseComparison() {
        auto first = ParseRange();
        if (!IsComparison(Peek())) {
            return first;
        }
        auto expr = MakeExpr(Expr::Kind::Compare, first->ref);
        expr->args.push_back(std::move(first));
        while (IsComparison(Peek())) {
            const Token& op = Advance();
            SkipNewlines();
            expr->ops.push_back(op.text == "in" ? "in" : op.text);
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
                return token.text != "mod" && token.text != "in";
            case Kind::Op:
                return token.text == "(" || token.text == "∞" || token.text == "∅";
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

    // Nothing can follow here: the end of a statement or of a bracket.
    static bool EndsValue(const Token& token) {
        return token.kind == Kind::Newline || token.kind == Kind::End || token.IsOp(";") ||
               token.IsOp(")") || token.IsOp(",") || token.IsOp("}") || token.IsOp("]");
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
            } else if (next.IsOp(".") && PeekAt(1).kind == Kind::Word) {
                Advance();
                const Token& member = Advance();
                const bool method = Peek().IsOp("(");
                auto access = MakeExpr(method ? Expr::Kind::Method : Expr::Kind::Member,
                                       Span(expr->ref, member.ref));
                access->text = member.text;
                access->opRef = member.ref;
                access->args.push_back(std::move(expr));
                if (method) {
                    Advance();
                    ++depth_;
                    if (!Peek().IsOp(")")) {
                        for (;;) {
                            access->args.push_back(ParseExpression());
                            if (!Peek().IsOp(",")) break;
                            Advance();
                        }
                    }
                    access->ref = Span(access->ref, Expect(")", "Expected ',' or ')'").ref);
                    --depth_;
                }
                expr = std::move(access);
            } else if (next.IsOp("[")) {
                // m["key"], L[i]: the same as a subscript.
                const Token& open = Advance();
                ++depth_;
                auto index = MakeExpr(Expr::Kind::Index, expr->ref);
                index->opRef = open.ref;
                index->args.push_back(std::move(expr));
                index->args.push_back(ParseExpression());
                const Token& close = Expect("]", "Expected ']'");
                --depth_;
                index->ref = Span(index->ref, close.ref);
                expr = std::move(index);
            } else if (next.IsOp("⋅") && EndsValue(PeekAt(1)) &&
                       (expr->kind == Expr::Kind::TypeName || expr->kind == Expr::Kind::Name ||
                        expr->kind == Expr::Kind::StructValue ||
                        expr->kind == Expr::Kind::StructType)) {
                // T*: a new, empty list of T.
                const Token& star = Advance();
                auto list = MakeExpr(Expr::Kind::ListOf, Span(expr->ref, star.ref));
                list->args.push_back(std::move(expr));
                expr = std::move(list);
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
                if (!token.slots.empty()) {
                    // "Hi ${name}!": text pieces with values between them.
                    auto expr = MakeExpr(Expr::Kind::Template, token.ref);
                    expr->ops = token.pieces;
                    for (const auto& slot : token.slots) {
                        SyntaxParser parser(slot, diagnostics_, token.ref);
                        expr->args.push_back(parser.ParseWholeExpression("${…}"));
                    }
                    return expr;
                }
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
                    return ParseSet();
                }
                if (token.IsOp("∅")) {
                    Advance();
                    return MakeExpr(Expr::Kind::EmptySet, token.ref);
                }
                if (token.IsOp("∀") || token.IsOp("∃")) {
                    return ParseQuantifier();
                }
                if (token.IsOp("∞")) {
                    Advance();
                    return MakeExpr(Expr::Kind::Infinity, token.ref);
                }
                if (IsBuiltinSet(token)) {
                    // A set as a value: only useful as T* (a new list), which
                    // the checker sees to.
                    Advance();
                    auto expr = MakeExpr(Expr::Kind::TypeName, token.ref);
                    expr->text = token.text;
                    if (token.IsOp("Σ") && Peek().IsOp("⋅") && PeekAt(1).IsOp("⋅")) {
                        expr->text = "Σ*";
                        expr->ref = Span(token.ref, Advance().ref);
                    }
                    return expr;
                }
                if (token.IsOp("[")) {
                    return ParseList();
                }
                if (token.IsOp("<")) {
                    // <name = "Bob", age = 4>, the typed form of ⟨…⟩.
                    const Token& open = Advance();
                    auto expr = MakeExpr(Expr::Kind::StructValue, open.ref);
                    ParseFields(*expr, true);
                    expr->ref = Span(open.ref, Expect(">", "Expected ',' or '>' after the fields").ref);
                    return expr;
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

    // `binding` is "x ∈ S": sets expr's variable and domain (args[0]).
    void Bind(Expr& expr, std::unique_ptr<Expr> binding, const char* example) {
        if (binding->kind != Expr::Kind::Compare || binding->ops.size() != 1 ||
            binding->ops[0] != "∈" || binding->args[0]->kind != Expr::Kind::Name) {
            Fail(std::string("Expected a variable and where it comes from, as in ") + example +
                     ".",
                 binding->ref);
        }
        const Expr& name = *binding->args[0];
        expr.var = name.text;
        expr.varIdent = name.ident;
        expr.varGreek = name.greek;
        expr.varRef = name.ref;
        expr.args.insert(expr.args.begin(), std::move(binding->args[1]));
    }

    // {a, b, c}, {1..n}, {}, or set-builder: {x ∈ S : P(x)}, {f(x) : x ∈ S},
    // {f(x) : x ∈ S, P(x)}. | may be used instead of :.
    std::unique_ptr<Expr> ParseSet() {
        const Token& open = Advance();
        ++depth_;
        if (Peek().IsOp("}")) {
            const Token& close = Advance();
            --depth_;
            return MakeExpr(Expr::Kind::EmptySet, Span(open.ref, close.ref));
        }
        auto first = ParseExpression();
        std::unique_ptr<Expr> set;
        if (Peek().IsOp(":") || Peek().IsOp("|")) {
            Advance();
            set = MakeExpr(Expr::Kind::Builder, open.ref);
            const bool filter = first->kind == Expr::Kind::Compare && first->ops.size() == 1 &&
                                first->ops[0] == "∈" &&
                                first->args[0]->kind == Expr::Kind::Name;
            if (filter) {
                // {x ∈ S : P}: the elements are x itself.
                auto element = Clone(*first->args[0]);
                Bind(*set, std::move(first), "{x ∈ S : x > 0}");
                set->args.push_back(std::move(element));
                set->conditions.push_back(ParseExpression());
            } else {
                set->args.push_back(std::move(first));
                Bind(*set, ParseExpression(), "{x² : x ∈ S}");
                if (Peek().IsOp(",")) {
                    Advance();
                    set->conditions.push_back(ParseExpression());
                } else {
                    set->conditions.push_back(nullptr);
                }
            }
        } else {
            set = MakeExpr(Expr::Kind::Set, open.ref);
            set->args.push_back(std::move(first));
            while (Peek().IsOp(",")) {
                Advance();
                set->args.push_back(ParseExpression());
            }
        }
        const Token& close = Expect("}", "Expected ',' or '}' in the set");
        --depth_;
        set->ref = Span(open.ref, close.ref);
        return set;
    }

    // ∀ x ∈ S : P(x), ∃ x ∈ S : P(x). The condition reaches as far right as
    // it can.
    std::unique_ptr<Expr> ParseQuantifier() {
        const Token& symbol = Advance();
        auto expr = MakeExpr(Expr::Kind::Quantifier, symbol.ref);
        expr->text = symbol.text;
        expr->opRef = symbol.ref;
        const Token& name = Peek();
        if (name.kind != Kind::Word || IsKeyword(name.text)) {
            Unexpected(name, "Expected a variable after " + symbol.text + ", as in " +
                                 symbol.text + " x ∈ S : x > 0");
        }
        Advance();
        expr->var = name.text;
        expr->varIdent = name.ident;
        expr->varGreek = name.greek;
        expr->varRef = name.ref;
        if (!Peek().IsOp("∈") && !Peek().IsWord("in")) {
            Unexpected(Peek(), "Expected ∈ after " + symbol.text + " " + name.text);
        }
        Advance();
        expr->args.push_back(ParseRange());
        if (!Peek().IsOp(":") && !Peek().IsOp(",")) {
            Unexpected(Peek(), "Expected ':' before the condition");
        }
        Advance();
        expr->args.push_back(ParseExpression());
        expr->ref = Span(symbol.ref, expr->args.back()->ref);
        return expr;
    }

    // Σ_{k=1}^{n} body or Σ_{k ∈ S} body: the body is the term after it
    // (Σ k² + 1 is (Σ k²) + 1).
    std::unique_ptr<Expr> ParseBigOp(const Token& token) {
        auto expr = MakeExpr(Expr::Kind::BigOp, token.ref);
        expr->text = token.text;
        expr->opRef = token.ref;
        auto lower = ParseSlot(token.slots[0], token, "lower limit");
        const bool from = lower->kind == Expr::Kind::Compare && lower->ops.size() == 1 &&
                          lower->ops[0] == "=" && lower->args[0]->kind == Expr::Kind::Name;
        if (from) {
            // k=a with an upper limit b: k runs through a..b.
            if (token.slots[1].empty()) {
                Fail("This needs an upper limit, as in k=1 up to n.", token.ref);
            }
            auto range = MakeExpr(Expr::Kind::Range, Span(lower->args[1]->ref, token.ref));
            range->args.push_back(std::move(lower->args[1]));
            range->args.push_back(ParseSlot(token.slots[1], token, "upper limit"));
            lower->args[1] = std::move(range);
            lower->ops[0] = "∈";
        } else if (!token.slots[1].empty()) {
            Fail("With ∈ below, leave the upper limit empty.", token.ref);
        }
        Bind(*expr, std::move(lower), "k=1 or k ∈ S");
        // (A set may follow too: ⋃_{k=1}^{n} {k, 2k}.)
        if (!StartsOperand(Peek()) && !Peek().IsOp("−") && !Peek().IsOp("{")) {
            Unexpected(Peek(), "Expected what to " +
                                   std::string(token.text == "sum" ? "add up" :
                                               token.text == "prod" ? "multiply" : "combine"));
        }
        expr->args.push_back(ParseMultiplicative());
        expr->ref = Span(token.ref, expr->args.back()->ref);
        return expr;
    }

    // [a, b, c]
    std::unique_ptr<Expr> ParseList() {
        const Token& open = Advance();
        ++depth_;
        auto list = MakeExpr(Expr::Kind::List, open.ref);
        if (Peek().IsOp("]")) {
            Fail("An empty list needs a type: write ℝ*, Σ** or Person*.", open.ref);
        }
        for (;;) {
            list->args.push_back(ParseExpression());
            if (!Peek().IsOp(",")) break;
            Advance();
        }
        const Token& close = Expect("]", "Expected ',' or ']' in the list");
        --depth_;
        list->ref = Span(open.ref, close.ref);
        return list;
    }

public:
    // The fields of ⟨…⟩ or <…>, separated by commas: name ∈ T @tags… (a
    // struct type) or name = value (a struct value). `typed` is the <…> form,
    // whose values stop before a comparison (the > that closes it).
    void ParseFields(Expr& expr, bool typed) {
        for (;;) {
            const Token& name = Peek();
            if (name.kind != Kind::Word || IsKeyword(name.text)) {
                Unexpected(name, "Expected the name of a field");
            }
            Advance();
            const Token& op = Peek();
            const bool declares = op.IsOp("∈");
            if (!declares && !op.IsOp("=")) {
                Unexpected(op, "Expected = (a value) or ∈ (a type) after " + name.text);
            }
            Advance();
            const Expr::Kind kind = declares ? Expr::Kind::StructType : Expr::Kind::StructValue;
            if ((!expr.fields.empty() || !expr.ops.empty()) && expr.kind != kind) {
                Fail("Write all the fields with ∈ (a struct type) or all with = (a value).",
                     op.ref);
            }
            expr.kind = kind;
            if (declares) {
                Param field{.name = name.text, .ident = name.ident, .greek = name.greek,
                            .ref = name.ref};
                ParseType("Expected a type such as ℝ or Σ* after ∈").ApplyTo(field);
                field.tags = ParseTags();
                expr.fields.push_back(std::move(field));
            } else {
                expr.ops.push_back(name.text);
                expr.args.push_back(typed ? ParseRange() : ParseExpression());
            }
            if (!Peek().IsOp(",")) {
                return;
            }
            Advance();
        }
    }

private:
    // ⟨…⟩: each row holds one or more fields.
    std::unique_ptr<Expr> ParseAngle(const Token& token) {
        auto expr = MakeExpr(Expr::Kind::StructValue, token.ref);
        expr->opRef = token.ref;
        bool any = false;
        for (const auto& cell : token.slots) {
            if (cell.empty()) {
                continue;
            }
            any = true;
            SyntaxParser parser(cell, diagnostics_, token.ref);
            parser.ParseFields(*expr, false);
            if (parser.Peek().kind != Kind::End) {
                parser.Unexpected(parser.Peek(), "Expected ',' or the end of the row");
            }
        }
        if (!any) {
            Fail("⟨ ⟩ needs fields, as in ⟨name = \"Bob\"⟩ or ⟨name ∈ Σ*⟩.", token.ref);
        }
        return expr;
    }

    // A matrix of numbers; a single column is a vector. A vmatrix (bars) is
    // its determinant.
    std::unique_ptr<Expr> ParseMatrix(const Token& token) {
        if (token.text == "angle") {
            return ParseAngle(token);
        }
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
            case Shape::BigOp: return ParseBigOp(token);
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
