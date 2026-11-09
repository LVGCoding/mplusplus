#include "parser.h"

#include "math/FracNode.h"
#include "math/GreekNode.h"
#include "math/LineNode.h"
#include "math/MatrixNode.h"
#include "math/SqrtNode.h"
#include "math/TextNode.h"

#include <cctype>
#include <cstddef>
#include <utility>

namespace {
bool IsDelimiter(char c) {
    return c == '{' || c == '}' || c == '[' || c == ']' || c == '&';
}

bool IsSpace(char c) {
    return std::isspace(static_cast<unsigned char>(c)) != 0;
}

bool IsLetter(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) != 0;
}

using NodeList = std::vector<std::unique_ptr<EditorNode>>;

// Where a sequence of nodes is being parsed; decides which token ends it.
enum class Context {
    TopLevel,     // ends only at end of input
    Group,        // ends at "}"
    OptionalArg,  // ends at "]"
    MatrixCell,   // ends at "&", "\\" or "\end"
};

// Recursive-descent parser over the token list. Every Parse* function that can
// fail restores `pos_` on failure so the caller can fall back to plain text.
class TokenParser {
public:
    explicit TokenParser(const std::vector<std::string>& tokens) : tokens_(tokens) {}

    NodeList ParseAll() {
        return ParseSequence(Context::TopLevel);
    }

private:
    const std::vector<std::string>& tokens_;
    std::size_t pos_ = 0;

    bool AtEnd() const {
        return pos_ >= tokens_.size();
    }

    const std::string& Peek() const {
        static const std::string empty;
        return AtEnd() ? empty : tokens_[pos_];
    }

    bool PeekIs(const char* token) const {
        return !AtEnd() && tokens_[pos_] == token;
    }

    void SkipWhitespace() {
        while (!AtEnd() && !tokens_[pos_].empty() && IsSpace(tokens_[pos_][0])) {
            ++pos_;
        }
    }

    bool EndsSequence(Context context) const {
        if (AtEnd()) {
            return true;
        }
        const std::string& token = tokens_[pos_];
        switch (context) {
        case Context::TopLevel:
            return false;
        case Context::Group:
            return token == "}";
        case Context::OptionalArg:
            return token == "]";
        case Context::MatrixCell:
            return token == "&" || token == "\\\\" || token == "\\end";
        }
        return false;
    }

    NodeList ParseSequence(Context context) {
        NodeList nodes;
        std::string pendingText;

        auto flushText = [&]() {
            if (!pendingText.empty()) {
                nodes.push_back(std::make_unique<TextNode>(std::move(pendingText)));
                pendingText.clear();
            }
        };
        auto push = [&](std::unique_ptr<EditorNode> node) {
            flushText();
            nodes.push_back(std::move(node));
        };

        while (!EndsSequence(context)) {
            const std::string& token = Peek();
            std::unique_ptr<EditorNode> node;

            if (token == "{") {
                if (auto group = ParseBracedGroup()) {
                    push(std::make_unique<LineNode>(std::move(*group), true));
                    continue;
                }
            } else if (token == "\\frac") {
                node = ParseFrac();
            } else if (token == "\\sqrt") {
                node = ParseSqrt();
            } else if (token == "\\begin") {
                node = ParseMatrix();
            } else if (token.size() > 1 && token[0] == '\\') {
                if (auto symbol = GreekNode::SymbolFromName(std::string_view(token).substr(1))) {
                    node = std::make_unique<GreekNode>(*symbol);
                    ++pos_;
                }
            }

            if (node) {
                push(std::move(node));
            } else {
                // Plain text, an unknown command, or a construct that failed to parse.
                pendingText += token;
                ++pos_;
            }
        }

        flushText();
        return nodes;
    }

    // Parses "{...}" and returns its contents, or nullptr (with pos_ unchanged)
    // if the brace is never closed.
    std::unique_ptr<NodeList> ParseBracedGroup() {
        const std::size_t start = pos_;
        if (!PeekIs("{")) {
            return nullptr;
        }
        ++pos_;
        auto contents = std::make_unique<NodeList>(ParseSequence(Context::Group));
        if (!PeekIs("}")) {
            pos_ = start;
            return nullptr;
        }
        ++pos_;
        return contents;
    }

    // A required command argument "{...}" as an unbraced LineNode.
    std::unique_ptr<EditorNode> ParseArgument() {
        SkipWhitespace();
        auto contents = ParseBracedGroup();
        if (!contents) {
            return nullptr;
        }
        return std::make_unique<LineNode>(std::move(*contents));
    }

    std::unique_ptr<EditorNode> ParseFrac() {
        const std::size_t start = pos_;
        ++pos_;  // \frac
        auto numerator = ParseArgument();
        auto denominator = numerator ? ParseArgument() : nullptr;
        if (!numerator || !denominator) {
            pos_ = start;
            return nullptr;
        }
        return std::make_unique<FracNode>(std::move(numerator), std::move(denominator));
    }

    std::unique_ptr<EditorNode> ParseSqrt() {
        const std::size_t start = pos_;
        ++pos_;  // \sqrt
        SkipWhitespace();

        std::unique_ptr<EditorNode> root;
        if (PeekIs("[")) {
            ++pos_;
            auto contents = ParseSequence(Context::OptionalArg);
            if (!PeekIs("]")) {
                pos_ = start;
                return nullptr;
            }
            ++pos_;
            root = std::make_unique<LineNode>(std::move(contents));
        }

        auto argument = ParseArgument();
        if (!argument) {
            pos_ = start;
            return nullptr;
        }
        return std::make_unique<SqrtNode>(std::move(root), std::move(argument));
    }

    // Reads "{name}" where name is a single text token, e.g. the "{pmatrix}"
    // after \begin. Returns an empty string (with pos_ unchanged) on failure.
    std::string ParseEnvironmentName() {
        const std::size_t start = pos_;
        SkipWhitespace();
        if (pos_ + 2 < tokens_.size() && tokens_[pos_] == "{" && tokens_[pos_ + 2] == "}") {
            std::string name = tokens_[pos_ + 1];
            pos_ += 3;
            return name;
        }
        pos_ = start;
        return {};
    }

    std::unique_ptr<EditorNode> ParseMatrix() {
        const std::size_t start = pos_;
        ++pos_;  // \begin
        const std::string environment = ParseEnvironmentName();
        if (!MatrixNode::IsMatrixEnvironment(environment)) {
            pos_ = start;
            return nullptr;
        }

        std::vector<MatrixNode::Row> rows(1);
        while (true) {
            rows.back().push_back(std::make_unique<LineNode>(ParseSequence(Context::MatrixCell)));
            if (AtEnd()) {
                break;  // missing \end
            }
            const std::string token = Peek();
            ++pos_;
            if (token == "&") {
                continue;
            }
            if (token == "\\\\") {
                rows.emplace_back();
                continue;
            }
            // token == "\end"
            if (ParseEnvironmentName() == environment) {
                return std::make_unique<MatrixNode>(environment, std::move(rows));
            }
            break;  // \end of a different environment
        }

        pos_ = start;
        return nullptr;
    }
};
}  // namespace

std::vector<std::unique_ptr<EditorNode>> Parser::ParseText(const std::string& inputText) {
    const std::vector<std::string> tokens = Tokenizer(inputText);
    return TokenParser(tokens).ParseAll();
}

std::vector<std::string> Parser::Tokenizer(const std::string& inputText) {
    std::vector<std::string> tokens;
    std::string currentToken;

    auto flush = [&]() {
        if (!currentToken.empty()) {
            tokens.push_back(std::move(currentToken));
            currentToken.clear();
        }
    };

    std::size_t index = 0;
    while (index < inputText.size()) {
        const char c = inputText[index];

        if (c == '\\') {
            // Command: "\name" or a single escaped character like "\\" or "\{".
            flush();
            std::size_t end = index + 1;
            if (end < inputText.size() && IsLetter(inputText[end])) {
                while (end < inputText.size() && IsLetter(inputText[end]))
                    ++end;
            } else if (end < inputText.size()) {
                ++end;
            }
            tokens.push_back(inputText.substr(index, end - index));
            index = end;
        } else if (IsDelimiter(c)) {
            flush();
            tokens.emplace_back(1, c);
            ++index;
        } else if (IsSpace(c)) {
            flush();
            std::size_t end = index;
            while (end < inputText.size() && IsSpace(inputText[end]))
                ++end;
            tokens.push_back(inputText.substr(index, end - index));
            index = end;
        } else {
            currentToken += c;
            ++index;
        }
    }
    flush();

    return tokens;
}
