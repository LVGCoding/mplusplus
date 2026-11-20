#pragma once

// Lowering: a document's node tree -> tokens (see Token.h).

#include "Token.h"

#include <memory>
#include <vector>

class LineNode;

namespace lang {

// Tokens of every row, each row followed by a Newline, then End. Rows must
// be normalized (see LineNode::Normalize), as a FileMpp's rows always are.
// Problems such as an unclosed string are added to `diagnostics`.
std::vector<Token> LowerDocument(const std::vector<std::unique_ptr<LineNode>>& rows,
                                 std::vector<Diagnostic>& diagnostics);

// Tokens of one line whose path in the document is `path` (for a row: {row}).
std::vector<Token> LowerLine(const LineNode& line,
                             const CursorPath& path,
                             std::vector<Diagnostic>& diagnostics);

}  // namespace lang
