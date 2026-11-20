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
//   \exp{base}{exponent}               -> ExpoNode (children are LineNodes)
//   \abs{x}, \floor{x}, \ceil{x},
//   \norm{x}                           -> DelimNode (|x|, ⌊x⌋, ⌈x⌉, ‖x‖)
//   _{i}                               -> SubNode (a subscript)
//   \sum_{k=1}^{n}, \prod, \bigcup, \bigcap
//                                      -> BigOpNode
//   \alpha ... \omega                  -> GreekNode
//   \in, \leftarrow, \mathbb{R}, ...   -> SymbolNode (see its table)
//   \begin{matrix} a & b \\ c & d \end{matrix}
//                                      -> MatrixNode (cells are LineNodes); also
//                                         pmatrix, bmatrix, Bmatrix, vmatrix, Vmatrix, cases
//   {...}                              -> LineNode with IsBraced() == true
//   \{ \} \_                           -> the text "{" / "}" / "_" (typed as text)
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
    //   - single-character delimiters: { } [ ] & _
    //   - runs of whitespace
    //   - runs of any other text
    // Concatenating the tokens reproduces the input exactly.
    std::vector<std::string> Tokenizer(const std::string& inputText);
};
