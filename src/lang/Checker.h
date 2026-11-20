#pragma once

// Checking: names and types. Fills in Expr::type and Stmt::type.

#include "Ast.h"

#include <vector>

namespace lang {

// Check `program`, adding a diagnostic for each problem. The program can
// only be compiled if this adds none.
void Check(Program& program, std::vector<Diagnostic>& diagnostics);

}  // namespace lang
