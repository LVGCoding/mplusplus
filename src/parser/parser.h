#pragma once

#include "math/EditorNode.h"

#include <memory>
#include <string>
#include <vector>

// Turns M++/LaTeX source into a tree of EditorNodes.
//
// Supported constructs:
//   \frac{a}{b}                        -> FracNode (children are LineNodes)
//   \sqrt{x}, \sqrt[n]{x}              -> SqrtNode (children are LineNodes; root may be null)
//   \alpha ... \omega                  -> GreekNode
//   \begin{matrix} a & b \\ c & d \end{matrix}
//                                      -> MatrixNode (cells are LineNodes); also
//                                         pmatrix, bmatrix, Bmatrix, vmatrix, Vmatrix
//   {...}                              -> LineNode with IsBraced() == true
//   anything else                      -> TextNode (adjacent text is merged)
//
// Parsing never fails: malformed input (e.g. "\frac{a}" or an unclosed "{") is
// kept as plain text, so no characters are lost. For input without whitespace
// between a command and its arguments, concatenating ToText() of the result
// reproduces the input exactly.
class Parser {
public:
    std::vector<std::unique_ptr<EditorNode>> ParseText(const std::string& inputText);

    // Split source into tokens:
    //   - commands: a backslash plus its name ("\frac", "\alpha"), or a backslash
    //     plus one non-letter character ("\\", "\{")
    //   - single-character delimiters: { } [ ] &
    //   - runs of whitespace
    //   - runs of any other text
    // Concatenating the tokens reproduces the input exactly.
    std::vector<std::string> Tokenizer(const std::string& inputText);
};
