
#include "parser.h"
#include "math/EditorNode.h"
#include <string>
#include <vector>

#include <cctype>
#include <cstddef>

namespace {
bool IsDelimiter(char c) {
  return c == '{' || c == '}' || c == '[' || c == ']';
}

bool IsSpace(char c) {
  return std::isspace(static_cast<unsigned char>(c)) != 0;
}

bool IsLetter(char c) {
  return std::isalpha(static_cast<unsigned char>(c)) != 0;
}
} // namespace

std::vector<std::unique_ptr<EditorNode>>
Parser::ParseText(const std::string &inputText) {
  std::vector<std::unique_ptr<EditorNode>> nodes;
  const std::vector<std::string> tokens = Tokenizer(inputText);

  // TODO: build TextNode/FracNode/SqrtNode/... from `tokens` once the node
  // classes have implementations (they're declared but not defined yet).
  (void)tokens;

  return nodes;
}

std::vector<std::string> Parser::Tokenizer(const std::string &inputText) {
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
