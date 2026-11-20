#include "Compile.h"

#include "Checker.h"
#include "Lower.h"
#include "Parser.h"

#include <algorithm>

namespace lang {

Compilation Compile(const std::vector<std::unique_ptr<LineNode>>& rows, CompileMode mode) {
    Compilation result;
    const std::vector<Token> tokens = LowerDocument(rows, result.diagnostics);
    Program program = Parse(tokens, result.diagnostics);
    Check(program, result.diagnostics);
    // Lowering, parsing and checking each report in document order; merge.
    std::stable_sort(result.diagnostics.begin(), result.diagnostics.end(),
                     [](const Diagnostic& a, const Diagnostic& b) {
                         return a.ref.start < b.ref.start;
                     });
    // A function is checked once per combination of argument types, so the
    // same problem in its body can be found more than once.
    result.diagnostics.erase(
        std::unique(result.diagnostics.begin(), result.diagnostics.end(),
                    [](const Diagnostic& a, const Diagnostic& b) {
                        return a.ref == b.ref && a.message == b.message;
                    }),
        result.diagnostics.end());
    if (result.Succeeded() && mode == CompileMode::Generate) {
        result.code = Generate(program);
    }
    return result;
}

}  // namespace lang
