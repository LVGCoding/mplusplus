#include "FileMpp.h"

#include "fileText.h"
#include "math/TextNode.h"
#include "parser/parser.h"

#include <utility>

namespace {
// Split the parser's flat output into one LineNode per source line. Only
// newlines in top-level text start a new row; newlines inside a node (e.g. a
// matrix written over several lines) belong to that node.
std::vector<std::unique_ptr<LineNode>> SplitIntoRows(std::vector<std::unique_ptr<EditorNode>> nodes,
                                                     bool& crlf) {
    std::vector<std::vector<std::unique_ptr<EditorNode>>> rows(1);
    for (auto& node : nodes) {
        auto* text = dynamic_cast<TextNode*>(node.get());
        if (!text) {
            rows.back().push_back(std::move(node));
            continue;
        }
        const std::string& whole = text->GetText();
        std::size_t start = 0;
        while (true) {
            const std::size_t newline = whole.find('\n', start);
            std::string line = whole.substr(start, newline - start);
            if (newline != std::string::npos && !line.empty() && line.back() == '\r') {
                line.pop_back();
                crlf = true;
            }
            if (!line.empty()) {
                rows.back().push_back(std::make_unique<TextNode>(std::move(line)));
            }
            if (newline == std::string::npos) {
                break;
            }
            rows.emplace_back();
            start = newline + 1;
        }
    }

    std::vector<std::unique_ptr<LineNode>> lines;
    lines.reserve(rows.size());
    for (auto& row : rows) {
        auto line = std::make_unique<LineNode>(std::move(row));
        line->Normalize();
        lines.push_back(std::move(line));
    }
    return lines;
}
}  // namespace

FileMpp::FileMpp() {
    SetSource({});
}

bool FileMpp::Load(const std::filesystem::path& path, std::string& error) {
    std::string text;
    if (!ReadWholeFile(path, text, error)) {
        return false;
    }
    SetSource(std::move(text));
    savedSource_ = source;
    this->path = path;
    modified = false;
    return true;
}

bool FileMpp::Save(const std::filesystem::path& path, std::string& error) {
    if (!WriteWholeFile(path, source, error)) {
        return false;
    }
    savedSource_ = source;
    this->path = path;
    modified = false;
    return true;
}

void FileMpp::SetSource(std::string text) {
    source = std::move(text);
    bool crlf = false;
    content = SplitIntoRows(Parser().ParseText(source), crlf);
    lineEnding_ = crlf ? "\r\n" : "\n";
    cursorPos = JoinPath({0}, content.front()->FirstPosition());
    selectionStart = cursorPos;
    goalX_.reset();
    pendingDelete_.reset();
    insertWheel_.reset();
    ClearHistory();
}

std::string FileMpp::ContentText() const {
    std::string text;
    for (std::size_t i = 0; i < content.size(); ++i) {
        if (i > 0) {
            text += lineEnding_;
        }
        text += content[i]->ToText();
    }
    return text;
}
