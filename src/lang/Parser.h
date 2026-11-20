#pragma once

// Parsing: tokens (from Lower) -> syntax tree (Ast.h).

#include "Ast.h"
#include "Token.h"

#include <vector>

namespace lang {

// Parse a whole document's tokens (from LowerDocument). A statement with a
// syntax error is reported in `diagnostics` and left out of the program; the
// rest are still parsed.
Program Parse(const std::vector<Token>& tokens, std::vector<Diagnostic>& diagnostics);

// True for words that can't be names: if, else, while, for, return, break,
// continue, in, mod, true, false.
bool IsKeyword(std::string_view word);

}  // namespace lang
