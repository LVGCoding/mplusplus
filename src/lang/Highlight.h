#pragma once

// Syntax highlighting from the tokens of each row (see Lower.h): keywords,
// numbers, strings, called functions, built-in sets and constants. Comments
// (after %) have no tokens; the editor draws them grey itself.

#include "math/Syntax.h"

#include <memory>
#include <vector>

class LineNode;

namespace lang {

// Replace `colors` with the colors of `rows` (a document's rows).
void HighlightDocument(const std::vector<std::unique_ptr<LineNode>>& rows, SyntaxColors& colors);

}  // namespace lang
