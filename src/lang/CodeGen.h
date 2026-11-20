#pragma once

// Code generation: a checked program -> C++ that uses the mpp runtime
// (runtime/include/mpp).

#include "Ast.h"

#include <string>
#include <vector>

namespace lang {

struct GeneratedCode {
    std::string cpp;
    // Places the program can stop with an error. The runtime reports an error
    // as "error:N: message", where N indexes this list.
    std::vector<SourceRef> locations;
};

// `program` must have passed Check without errors.
GeneratedCode Generate(const Program& program);

}  // namespace lang
