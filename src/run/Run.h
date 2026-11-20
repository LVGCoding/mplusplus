#pragma once

#include <string>
#include <vector>

struct FileBase;

// Running a document: Run -> Run translates the active file to C++ with
// TranslateFile, then a run::Job (Driver.h) compiles and runs it in the
// background and streams its output to the terminal panel.
//
// The language itself (lowering, parsing, checking, code generation) will
// live in src/lang/; see docs/language-plan.md.

struct Translation {
    // False if the document can't be run; `errors` explains why.
    bool succeeded = true;
    // A complete C++ program using the mpp runtime (runtime/include/mpp).
    std::string cpp;
    std::vector<std::string> errors;
};

Translation TranslateFile(const FileBase& file);
