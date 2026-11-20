#pragma once

// The whole language pipeline: document rows -> tokens -> syntax tree ->
// checked program -> C++. See docs/language-plan.md.

#include "CodeGen.h"
#include "Source.h"

#include <memory>
#include <vector>

class LineNode;

namespace lang {

struct Compilation {
    // Problems found, in document order. If there are any, `code` is empty.
    std::vector<Diagnostic> diagnostics;
    GeneratedCode code;

    bool Succeeded() const { return diagnostics.empty(); }
};

enum class CompileMode { Generate, CheckOnly };

// Compile a document's rows (normalized, as a FileMpp's rows are).
// CheckOnly stops after checking, for showing errors while editing.
Compilation Compile(const std::vector<std::unique_ptr<LineNode>>& rows,
                    CompileMode mode = CompileMode::Generate);

}  // namespace lang
