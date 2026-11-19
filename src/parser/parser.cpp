#include "parser.h"

#include "math/FracNode.h"
#include "math/GraphNode.h"
#include "math/GreekNode.h"
#include "math/LineNode.h"
#include "math/MatrixNode.h"
#include "math/Options.h"
#include "math/SqrtNode.h"
#include "math/TableNode.h"
#include "math/TextNode.h"

#include <cctype>
#include <cstddef>
#include <utility>

namespace {
bool IsDelimiter(char character) {
    return character == '{' || character == '}' || character == '[' || character == ']' ||
           character == '&';
}

bool IsSpace(char character) {
    return std::isspace(static_cast<unsigned char>(character)) != 0;
}

bool IsLetter(char character) {
    return std::isalpha(static_cast<unsigned char>(character)) != 0;
}

using NodeList = std::vector<std::unique_ptr<EditorNode>>;

// Where a sequence of nodes is being parsed; decides which token ends it.
enum class Context {
    TOP_LEVEL,     // ends only at end of input
    GROUP,         // ends at "}"
    OPTIONAL_ARG,  // ends at "]"
    MATRIX_CELL,   // ends at "&", "\\" or "\end"
};

// Recursive-descent parser over the token list. Command parsers restore position_
// on failure so ParseSequence() can preserve the malformed command as plain text.
class TokenParser {
public:
    explicit TokenParser(const std::vector<std::string>& tokens) : tokens_(tokens) {}

    NodeList ParseAll() {
        return ParseSequence(Context::TOP_LEVEL);
    }

private:
    const std::vector<std::string>& tokens_;
    std::size_t position_ = 0;

    bool AtEnd() const {
        return position_ >= tokens_.size();
    }

    const std::string& Peek() const {
        static const std::string EMPTY_TOKEN;
        return AtEnd() ? EMPTY_TOKEN : tokens_[position_];
    }

    bool PeekIs(const char* token) const {
        return !AtEnd() && tokens_[position_] == token;
    }

    void SkipWhitespace() {
        while (!AtEnd() && !tokens_[position_].empty() && IsSpace(tokens_[position_][0])) {
            ++position_;
        }
    }

    bool EndsSequence(Context context) const {
        if (AtEnd()) {
            return true;
        }
        const std::string& token = tokens_[position_];
        switch (context) {
        case Context::TOP_LEVEL:
            return false;
        case Context::GROUP:
            return token == "}";
        case Context::OPTIONAL_ARG:
            return token == "]";
        case Context::MATRIX_CELL:
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
                node = ParseGraph();
                if (!node) {
                    node = ParseTable();
                }
                if (!node) {
                    node = ParseMatrix();
                }
            } else if (token.size() > 1 && token[0] == '\\') {
                if (auto symbol = GreekNode::SymbolFromName(std::string_view(token).substr(1))) {
                    node = std::make_unique<GreekNode>(*symbol);
                    ++position_;
                }
            }

            if (node) {
                push(std::move(node));
            } else {
                // Plain text, an unknown command, or a construct that failed to parse.
                pendingText += token;
                ++position_;
            }
        }

        flushText();
        return nodes;
    }

    // Parses "{...}" and returns its contents, or nullptr (with position_ unchanged)
    // if the brace is never closed.
    std::unique_ptr<NodeList> ParseBracedGroup() {
        const std::size_t start = position_;
        if (!PeekIs("{")) {
            return nullptr;
        }
        ++position_;
        auto contents = std::make_unique<NodeList>(ParseSequence(Context::GROUP));
        if (!PeekIs("}")) {
            position_ = start;
            return nullptr;
        }
        ++position_;
        return contents;
    }

    // A required command argument "{...}" as an unbraced LineNode. The enclosing
    // command parser restores skipped whitespace if any required argument is missing.
    std::unique_ptr<EditorNode> ParseArgument() {
        SkipWhitespace();
        auto contents = ParseBracedGroup();
        if (!contents) {
            return nullptr;
        }
        return std::make_unique<LineNode>(std::move(*contents));
    }

    std::unique_ptr<EditorNode> ParseFrac() {
        const std::size_t start = position_;
        ++position_;  // \frac
        auto numerator = ParseArgument();
        auto denominator = numerator ? ParseArgument() : nullptr;
        if (!numerator || !denominator) {
            position_ = start;
            return nullptr;
        }
        return std::make_unique<FracNode>(std::move(numerator), std::move(denominator));
    }

    std::unique_ptr<EditorNode> ParseSqrt() {
        const std::size_t start = position_;
        ++position_;  // \sqrt
        SkipWhitespace();

        std::unique_ptr<EditorNode> root;
        if (PeekIs("[")) {
            ++position_;
            auto contents = ParseSequence(Context::OPTIONAL_ARG);
            if (!PeekIs("]")) {
                position_ = start;
                return nullptr;
            }
            ++position_;
            root = std::make_unique<LineNode>(std::move(contents));
        }

        auto argument = ParseArgument();
        if (!argument) {
            position_ = start;
            return nullptr;
        }
        return std::make_unique<SqrtNode>(std::move(root), std::move(argument));
    }

    // Reads "{name}" where name is a single text token, e.g. the "{pmatrix}"
    // after \begin. Returns an empty string (with position_ unchanged) on failure.
    std::string ParseEnvironmentName() {
        const std::size_t start = position_;
        SkipWhitespace();
        if (position_ + 2 < tokens_.size() && tokens_[position_] == "{" &&
            tokens_[position_ + 2] == "}") {
            std::string name = tokens_[position_ + 1];
            position_ += 3;
            return name;
        }
        position_ = start;
        return {};
    }

    // Math stored as an option value (a title or axis label), as an editable line.
    static std::unique_ptr<EditorNode> LineFrom(const std::string& text) {
        return std::make_unique<LineNode>(Parser().ParseText(text));
    }

    // An object's optional "[options]", read as raw text (braces included) for
    // its settings to interpret (see Options.h). Absent options read as "".
    // Returns false for an unclosed "[" or unbalanced braces; the caller then
    // restores position_.
    bool ParseRawOptions(std::string& options) {
        options.clear();
        if (!PeekIs("[")) {
            return true;
        }
        ++position_;
        int depth = 0;
        while (!AtEnd()) {
            const std::string& token = tokens_[position_++];
            if (token == "]" && depth == 0) {
                return true;
            }
            if (token == "{") {
                ++depth;
            } else if (token == "}" && --depth < 0) {
                return false;
            }
            options += token;
        }
        return false;  // unclosed "["
    }

    // \begin{graph}[options] f1 \\ f2 \end{graph}
    std::unique_ptr<EditorNode> ParseGraph() {
        const std::size_t start = position_;
        ++position_;  // \begin
        std::string options;
        if (ParseEnvironmentName() != GraphNode::ENVIRONMENT || !ParseRawOptions(options)) {
            position_ = start;
            return nullptr;
        }

        std::vector<std::unique_ptr<EditorNode>> functions;
        while (true) {
            functions.push_back(std::make_unique<LineNode>(ParseSequence(Context::MATRIX_CELL)));
            if (AtEnd()) {
                break;  // missing \end
            }
            const std::string token = Peek();
            ++position_;
            if (token == "\\\\") {
                continue;
            }
            if (token == "\\end" && ParseEnvironmentName() == GraphNode::ENVIRONMENT) {
                // The title and axis labels are editable math; the rest are settings.
                std::unique_ptr<EditorNode> title;
                std::unique_ptr<EditorNode> xLabel = LineFrom("x");
                std::unique_ptr<EditorNode> yLabel = LineFrom("y");
                std::vector<options::Option> rest;
                for (auto& [key, value] : options::Parse(options)) {
                    if (key == "title") {
                        title = LineFrom(value);
                    } else if (key == "xlabel") {
                        xLabel = LineFrom(value);
                    } else if (key == "ylabel") {
                        yLabel = LineFrom(value);
                    } else {
                        rest.emplace_back(std::move(key), std::move(value));
                    }
                }
                return std::make_unique<GraphNode>(GraphSettings::FromOptions(rest),
                                                   std::move(title),
                                                   std::move(xLabel),
                                                   std::move(yLabel),
                                                   std::move(functions));
            }
            break;  // "&", or \end of a different environment
        }

        position_ = start;
        return nullptr;
    }

    // \begin{table}[options] a & b \\ c & d \end{table}
    std::unique_ptr<EditorNode> ParseTable() {
        const std::size_t start = position_;
        ++position_;  // \begin
        std::string options;
        if (ParseEnvironmentName() != TableNode::ENVIRONMENT || !ParseRawOptions(options)) {
            position_ = start;
            return nullptr;
        }

        std::vector<MatrixNode::Row> rows(1);
        while (true) {
            rows.back().push_back(std::make_unique<LineNode>(ParseSequence(Context::MATRIX_CELL)));
            if (AtEnd()) {
                break;  // missing \end
            }
            const std::string token = Peek();
            ++position_;
            if (token == "&") {
                continue;
            }
            if (token == "\\\\") {
                rows.emplace_back();
                continue;
            }
            if (token == "\\end" && ParseEnvironmentName() == TableNode::ENVIRONMENT) {
                std::unique_ptr<EditorNode> title;
                bool header = false;
                std::vector<options::Option> unknown;
                for (auto& [key, value] : options::Parse(options)) {
                    if (key == "title") {
                        title = LineFrom(value);
                    } else if (key == "header") {
                        options::ParseBool(value, header);
                    } else {
                        unknown.emplace_back(std::move(key), std::move(value));
                    }
                }
                return std::make_unique<TableNode>(
                    std::move(title), header, std::move(unknown), std::move(rows));
            }
            break;  // \end of a different environment
        }

        position_ = start;
        return nullptr;
    }

    std::unique_ptr<EditorNode> ParseMatrix() {
        const std::size_t start = position_;
        ++position_;  // \begin
        const std::string environment = ParseEnvironmentName();
        if (!MatrixNode::IsMatrixEnvironment(environment)) {
            position_ = start;
            return nullptr;
        }

        std::vector<MatrixNode::Row> rows(1);
        while (true) {
            rows.back().push_back(std::make_unique<LineNode>(ParseSequence(Context::MATRIX_CELL)));
            if (AtEnd()) {
                break;  // missing \end
            }
            const std::string token = Peek();
            ++position_;
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

        position_ = start;
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

    auto flushToken = [&]() {
        if (!currentToken.empty()) {
            tokens.push_back(std::move(currentToken));
            currentToken.clear();
        }
    };

    std::size_t index = 0;
    while (index < inputText.size()) {
        const char character = inputText[index];

        if (character == '\\') {
            // Command: "\name" or a single escaped character like "\\" or "\{".
            flushToken();
            std::size_t end = index + 1;
            if (end < inputText.size() && IsLetter(inputText[end])) {
                while (end < inputText.size() && IsLetter(inputText[end])) {
                    ++end;
                }
            } else if (end < inputText.size()) {
                ++end;
            }
            tokens.push_back(inputText.substr(index, end - index));
            index = end;
        } else if (IsDelimiter(character)) {
            flushToken();
            tokens.emplace_back(1, character);
            ++index;
        } else if (IsSpace(character)) {
            flushToken();
            std::size_t end = index;
            while (end < inputText.size() && IsSpace(inputText[end])) {
                ++end;
            }
            tokens.push_back(inputText.substr(index, end - index));
            index = end;
        } else {
            currentToken += character;
            ++index;
        }
    }
    flushToken();

    return tokens;
}
