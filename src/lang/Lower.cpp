#include "Lower.h"

#include "math/BigOpNode.h"
#include "math/DelimNode.h"
#include "math/ExpoNode.h"
#include "math/FracNode.h"
#include "math/GraphNode.h"
#include "math/GreekNode.h"
#include "math/LineNode.h"
#include "math/MatrixNode.h"
#include "math/SqrtNode.h"
#include "math/SubNode.h"
#include "math/SymbolNode.h"
#include "math/TableNode.h"
#include "math/TextNode.h"

#include <array>
#include <optional>
#include <string_view>
#include <utility>

namespace lang {

SourceRef Span(const SourceRef& first, const SourceRef& last) {
    if (first.Empty() || last.Empty()) {
        return first.Empty() ? last : first;
    }
    const CursorPath& a = first.start;
    const CursorPath& b = last.end;
    const bool sameLine =
        a.size() == b.size() && std::equal(a.begin(), a.end() - 2, b.begin());
    const bool bothInRows = a.size() == 3 && b.size() == 3;
    if (sameLine || bothInRows) {
        return {a, b};
    }
    return first;
}

namespace {

using Kind = Token::Kind;

// Operators typed as text, longest first, with their canonical spelling.
struct Spelling {
    std::string_view typed;
    std::string_view canonical;
};
constexpr std::array<Spelling, 41> OPERATORS = {{
    {":=", ":="}, {"<-", "←"}, {"->", "→"}, {"<=", "≤"}, {">=", "≥"}, {"!=", "≠"},
    {"..", ".."},
    {"+", "+"},   {"-", "−"},  {"*", "⋅"},  {"/", "/"},  {"^", "^"},  {"(", "("},
    {")", ")"},   {"[", "["},  {"]", "]"},  {",", ","},  {";", ";"},  {":", ":"},
    {"=", "="},   {"<", "<"},  {">", ">"},  {"!", "!"},  {"|", "|"},  {"{", "{"},
    {"}", "}"},
    // Unicode typed directly.
    {"−", "−"},   {"·", "⋅"},  {"⋅", "⋅"},  {"×", "×"},  {"÷", "/"},  {"≤", "≤"},
    {"≥", "≥"},   {"≠", "≠"},  {"≈", "≈"},  {"←", "←"},  {"→", "→"},  {"∈", "∈"},
    {"∧", "∧"},   {"∨", "∨"},  {"¬", "¬"},
}};

bool IsDigit(char c) { return c >= '0' && c <= '9'; }
bool IsLetter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
bool IsSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

// Length in bytes of the UTF-8 character starting with `lead`.
std::size_t CharLength(unsigned char lead) {
    if (lead < 0x80) return 1;
    if (lead >> 5 == 0x6) return 2;
    if (lead >> 4 == 0xE) return 3;
    if (lead >> 3 == 0x1E) return 4;
    return 1;
}

class LineLowerer {
public:
    LineLowerer(const CursorPath& path, std::vector<Token>& out, std::vector<Diagnostic>& diagnostics)
        : path_(path), out_(out), diagnostics_(diagnostics) {}

    void Lower(const LineNode& line) {
        line_ = &line;
        for (std::size_t i = 0; i < line.ChildCount(); ++i) {
            const EditorNode* child = line.GetChild(i);
            if (const auto* text = dynamic_cast<const TextNode*>(child)) {
                LowerText(i, text->GetText());
            } else if (const auto* greek = dynamic_cast<const GreekNode*>(child)) {
                const std::string_view name = GreekNode::SymbolName(greek->GetSymbol());
                std::string glyph = GlyphOf(*greek);
                if (string_) {
                    string_->text += glyph;
                    continue;
                }
                Token token{.kind = Kind::Word, .text = glyph, .ident = std::string(name),
                            .greek = true, .ref = ChildRef(i)};
                out_.push_back(std::move(token));
            } else if (const auto* symbol = dynamic_cast<const SymbolNode*>(child)) {
                const std::string glyph(symbol->GetSymbol().glyph);
                if (string_) {
                    string_->text += glyph;
                    continue;
                }
                out_.push_back({.kind = Kind::Op, .text = glyph, .ref = ChildRef(i)});
            } else {
                if (string_) {
                    diagnostics_.push_back(
                        {"Only text can go inside a string (\"…\").", ChildRef(i)});
                    continue;
                }
                LowerStructure(i, *child);
            }
        }
        if (string_) {
            diagnostics_.push_back({"This string has no closing quote (\").", string_->ref});
            string_.reset();
        }
    }

private:
    const CursorPath& path_;
    std::vector<Token>& out_;
    std::vector<Diagnostic>& diagnostics_;
    const LineNode* line_ = nullptr;
    // A string still open at the end of the last text: it continues through
    // Greek letters and symbols (which become part of it) to its closing quote.
    std::optional<Token> string_;

    CursorPath At(std::size_t child, std::size_t offset) const {
        CursorPath position = path_;
        position.push_back(child);
        position.push_back(offset);
        return position;
    }

    // The range covering the non-text child at `index`: from the end of the
    // text before it to the start of the text after it.
    SourceRef ChildRef(std::size_t index) const {
        const auto* before =
            index > 0 ? dynamic_cast<const TextNode*>(line_->GetChild(index - 1)) : nullptr;
        SourceRef ref;
        ref.start = before ? At(index - 1, before->GetText().size()) : At(index, 0);
        ref.end = At(index + 1, 0);
        return ref;
    }

    static std::string GlyphOf(const GreekNode& greek) {
        // GreekNode doesn't expose its glyph; α..ω are consecutive codepoints
        // except that ς (final sigma) sits between ρ and σ.
        int index = static_cast<int>(greek.GetSymbol());
        char32_t codepoint = U'α' + static_cast<char32_t>(index);
        if (greek.GetSymbol() >= GreekNode::GREEK_SIGMA) {
            ++codepoint;
        }
        std::string glyph;
        glyph += static_cast<char>(0xC0 | (codepoint >> 6));
        glyph += static_cast<char>(0x80 | (codepoint & 0x3F));
        return glyph;
    }

    void Push(Kind kind, std::string text, std::size_t child, std::size_t begin, std::size_t end) {
        Token token{.kind = kind, .text = std::move(text)};
        token.ref = {At(child, begin), At(child, end)};
        if (kind == Kind::Word) {
            token.ident = token.text;
        }
        out_.push_back(std::move(token));
    }

    void LowerText(std::size_t child, const std::string& text) {
        std::size_t i = 0;
        const std::size_t n = text.size();
        while (i < n) {
            if (string_) {
                const std::size_t quote = text.find('"', i);
                if (quote == std::string::npos) {
                    string_->text += text.substr(i);
                    return;
                }
                string_->text += text.substr(i, quote - i);
                string_->ref.end = At(child, quote + 1);
                out_.push_back(std::move(*string_));
                string_.reset();
                i = quote + 1;
                continue;
            }
            const char c = text[i];
            if (IsSpace(c)) {
                ++i;
            } else if (c == '"') {
                string_ = Token{.kind = Kind::String};
                string_->ref = {At(child, i), At(child, i + 1)};
                ++i;
            } else if (IsDigit(c) || (c == '.' && i + 1 < n && IsDigit(text[i + 1]))) {
                const std::size_t begin = i;
                while (i < n && IsDigit(text[i])) ++i;
                // A '.' starts the fraction unless it's a range: 1..n
                if (i < n && text[i] == '.' && !(i + 1 < n && text[i + 1] == '.')) {
                    ++i;
                    while (i < n && IsDigit(text[i])) ++i;
                }
                if (i < n && (text[i] == 'e' || text[i] == 'E')) {
                    std::size_t j = i + 1;
                    if (j < n && (text[j] == '+' || text[j] == '-')) ++j;
                    if (j < n && IsDigit(text[j])) {
                        i = j;
                        while (i < n && IsDigit(text[i])) ++i;
                    }
                }
                Push(Kind::Number, text.substr(begin, i - begin), child, begin, i);
            } else if (IsLetter(c)) {
                const std::size_t begin = i;
                while (i < n && (IsLetter(text[i]) || IsDigit(text[i]))) ++i;
                Push(Kind::Word, text.substr(begin, i - begin), child, begin, i);
            } else {
                bool matched = false;
                for (const Spelling& op : OPERATORS) {
                    if (std::string_view(text).substr(i).starts_with(op.typed)) {
                        Push(Kind::Op, std::string(op.canonical), child, i, i + op.typed.size());
                        i += op.typed.size();
                        matched = true;
                        break;
                    }
                }
                if (!matched) {
                    const std::size_t length = std::min(CharLength(c), n - i);
                    diagnostics_.push_back({"M++ doesn't know what '" + text.substr(i, length) +
                                                "' means here.",
                                            {At(child, i), At(child, i + length)}});
                    i += length;
                }
            }
        }
    }

    // Tokens of the slot of a structure, at `slotPath` relative to the line.
    std::vector<Token> Slot(const EditorNode* node, std::initializer_list<std::size_t> slotPath) {
        std::vector<Token> tokens;
        const auto* line = dynamic_cast<const LineNode*>(node);
        if (!line) {
            return tokens;
        }
        CursorPath path = path_;
        path.insert(path.end(), slotPath);
        LineLowerer(path, tokens, diagnostics_).Lower(*line);
        return tokens;
    }

    void LowerStructure(std::size_t index, const EditorNode& node) {
        Token token{.kind = Kind::Structure, .ref = ChildRef(index)};
        if (const auto* fraction = dynamic_cast<const FracNode*>(&node)) {
            auto [numerator, denominator] = fraction->GetChildren();
            token.shape = Token::Shape::Fraction;
            token.slots.push_back(Slot(numerator, {index, 0}));
            token.slots.push_back(Slot(denominator, {index, 1}));
        } else if (const auto* root = dynamic_cast<const SqrtNode*>(&node)) {
            auto [degree, argument] = root->GetChildren();
            token.shape = Token::Shape::Root;
            token.slots.push_back(Slot(argument, {index, 1}));
            if (degree) {
                token.slots.push_back(Slot(degree, {index, 0}));
            }
        } else if (const auto* power = dynamic_cast<const ExpoNode*>(&node)) {
            token.shape = Token::Shape::Power;
            token.slots.push_back(Slot(power->GetBase(), {index, 0}));
            token.slots.push_back(Slot(power->GetExponent(), {index, 1}));
        } else if (dynamic_cast<const GraphNode*>(&node)) {
            token.shape = Token::Shape::Graph;
        } else if (dynamic_cast<const TableNode*>(&node)) {
            token.shape = Token::Shape::Table;
        } else if (const auto* delimited = dynamic_cast<const DelimNode*>(&node)) {
            token.shape = Token::Shape::Delimited;
            token.text = std::string(DelimNode::CommandName(delimited->GetKind()));
            token.slots.push_back(Slot(delimited->GetArgument(), {index, 0}));
        } else if (const auto* big = dynamic_cast<const BigOpNode*>(&node)) {
            token.shape = Token::Shape::BigOp;
            token.text = std::string(BigOpNode::CommandName(big->GetKind()));
            token.slots.push_back(Slot(big->GetLower(), {index, 0}));
            token.slots.push_back(Slot(big->GetUpper(), {index, 1}));
        } else if (const auto* subscript = dynamic_cast<const SubNode*>(&node)) {
            token.shape = Token::Shape::Subscript;
            token.slots.push_back(Slot(subscript->GetIndex(), {index, 0}));
        } else if (const auto* matrix = dynamic_cast<const MatrixNode*>(&node)) {
            token.text = matrix->GetEnvironment();
            token.shape = matrix->GetEnvironment() == "cases" ? Token::Shape::Cases
                                                             : Token::Shape::Matrix;
            for (std::size_t row = 0; row < matrix->RowCount(); ++row) {
                token.rowLengths.push_back(matrix->ColumnCount(row));
                for (std::size_t column = 0; column < matrix->ColumnCount(row); ++column) {
                    token.slots.push_back(Slot(matrix->GetCell(row, column), {index, row, column}));
                }
            }
        } else if (const auto* group = dynamic_cast<const LineNode*>(&node)) {
            token.shape = Token::Shape::Group;
            token.slots.push_back(Slot(group, {index}));
        } else {
            diagnostics_.push_back({"This can't be run yet.", token.ref});
            return;
        }
        out_.push_back(std::move(token));
    }
};

}  // namespace

std::vector<Token> LowerLine(const LineNode& line,
                             const CursorPath& path,
                             std::vector<Diagnostic>& diagnostics) {
    std::vector<Token> tokens;
    LineLowerer(path, tokens, diagnostics).Lower(line);
    return tokens;
}

std::vector<Token> LowerDocument(const std::vector<std::unique_ptr<LineNode>>& rows,
                                 std::vector<Diagnostic>& diagnostics) {
    std::vector<Token> tokens;
    for (std::size_t row = 0; row < rows.size(); ++row) {
        const CursorPath path{row};
        LineLowerer(path, tokens, diagnostics).Lower(*rows[row]);
        Token newline{.kind = Kind::Newline};
        const CursorPath end = JoinPath({row}, rows[row]->LastPosition());
        newline.ref = {end, end};
        tokens.push_back(std::move(newline));
    }
    Token end{.kind = Kind::End};
    if (!tokens.empty()) {
        end.ref = tokens.back().ref;
    }
    tokens.push_back(std::move(end));
    return tokens;
}

}  // namespace lang
