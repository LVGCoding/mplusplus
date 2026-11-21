#include "Highlight.h"

#include "Lower.h"
#include "Parser.h"
#include "math/LineNode.h"
#include "math/TextNode.h"

#include <algorithm>
#include <string_view>

namespace lang {
namespace {

using Kind = Token::Kind;

class Highlighter {
public:
    Highlighter(const std::vector<std::unique_ptr<LineNode>>& rows, SyntaxColors& colors)
        : rows_(rows), colors_(colors) {}

    void Tokens(const std::vector<Token>& tokens) {
        for (std::size_t i = 0; i < tokens.size(); ++i) {
            const Token& token = tokens[i];
            switch (token.kind) {
            case Kind::Number:
                Color(token.ref, SyntaxKind::Number);
                break;
            case Kind::Word:
                if (IsKeyword(token.text) || token.text == "otherwise") {
                    Color(token.ref, SyntaxKind::Keyword);
                } else if (i + 1 < tokens.size() && tokens[i + 1].IsOp("(")) {
                    Color(token.ref, SyntaxKind::Function);
                }
                break;
            case Kind::String:
                Color(token.ref, SyntaxKind::String);
                // The code in each ${…} is drawn as code, not in the
                // string's color: from its first token to the End token
                // at its closing brace.
                for (const auto& hole : token.slots) {
                    if (!hole.empty()) {
                        Color({hole.front().ref.start, hole.back().ref.start},
                              SyntaxKind::Plain);
                    }
                }
                break;  // the tokens in the holes (slots) are colored below
            case Kind::Op:
                if (IsBuiltinSet(token.text)) {
                    Color(token.ref, SyntaxKind::Type);
                } else if (token.text == "∞" || token.text == "∅") {
                    Color(token.ref, SyntaxKind::Constant);
                }
                break;
            default:
                break;
            }
            for (const auto& slot : token.slots) {
                Tokens(slot);
            }
        }
    }

private:
    const std::vector<std::unique_ptr<LineNode>>& rows_;
    SyntaxColors& colors_;

    static bool IsBuiltinSet(std::string_view op) {
        for (std::string_view set : {"ℕ", "ℤ", "ℚ", "ℝ", "ℂ", "𝔹", "Σ"}) {
            if (op == set) {
                return true;
            }
        }
        return false;
    }

    // The line holding the position `path` ({row, ..., child, offset}).
    const LineNode* LineAt(const CursorPath& path) const {
        if (path.size() < 3 || path[0] >= rows_.size()) {
            return nullptr;
        }
        const EditorNode* node = rows_[path[0]].get();
        PathView rest(path.data() + 1, path.size() - 3);
        while (node && !rest.empty()) {
            std::size_t used = 0;
            node = node->PathChild(rest, used);
            if (used == 0) {
                return nullptr;
            }
            rest = rest.subspan(used);
        }
        return dynamic_cast<const LineNode*>(node);
    }

    // Color the range `ref` within one line: parts of its text, and the
    // Greek letters and symbols inside it.
    void Color(const SourceRef& ref, SyntaxKind kind) {
        const CursorPath& start = ref.start;
        const CursorPath& end = ref.end;
        if (start.size() < 3 || start.size() != end.size() ||
            !std::equal(start.begin(), start.end() - 2, end.begin())) {
            return;
        }
        const LineNode* line = LineAt(start);
        if (!line) {
            return;
        }
        const std::size_t first = start[start.size() - 2];
        const std::size_t from = start.back();
        const std::size_t last = end[end.size() - 2];
        const std::size_t to = end.back();
        for (std::size_t c = first; c <= last && c < line->ChildCount(); ++c) {
            const EditorNode* child = line->GetChild(c);
            if (const auto* text = dynamic_cast<const TextNode*>(child)) {
                const std::size_t size = text->GetText().size();
                auto& kinds = colors_.text[child];
                kinds.resize(size, SyntaxKind::Plain);
                const std::size_t begin = c == first ? std::min(from, size) : 0;
                const std::size_t stop = c == last ? std::min(to, size) : size;
                std::fill(kinds.begin() + static_cast<std::ptrdiff_t>(begin),
                          kinds.begin() + static_cast<std::ptrdiff_t>(std::max(begin, stop)),
                          kind);
            } else if ((c > first || from == 0) && c < last) {
                colors_.nodes[child] = kind;
            }
        }
    }
};

}  // namespace

void HighlightDocument(const std::vector<std::unique_ptr<LineNode>>& rows, SyntaxColors& colors) {
    colors.Clear();
    Highlighter highlighter(rows, colors);
    std::vector<Diagnostic> ignored;
    for (std::size_t row = 0; row < rows.size(); ++row) {
        highlighter.Tokens(LowerLine(*rows[row], CursorPath{row}, ignored));
    }
}

}  // namespace lang
