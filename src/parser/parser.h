#pragma once

#include <memory>
#include <string>
#include <vector>

#include "math/EditorNode.h"

class Parser {
public:
    // Parse M++/LaTeX source into a list of editor nodes.
    std::vector<std::unique_ptr<EditorNode>> ParseText(const std::string& inputText);

    // Split source into tokens:
    //   - commands: a backslash plus its name ("\frac", "\alpha"), or a backslash
    //     plus one non-letter character ("\\", "\{")
    //   - single-character delimiters: { } [ ]
    //   - runs of whitespace
    //   - runs of any other text
    // Concatenating the tokens reproduces the input exactly.
    std::vector<std::string> Tokenizer(const std::string& inputText);
};
