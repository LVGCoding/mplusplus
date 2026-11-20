#pragma once

#include "lang/Source.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct FileBase;

// Running a document: Run -> Run translates the active file to C++ with
// TranslateFile (the language itself is in src/lang/), then a run::Job
// (Driver.h) compiles and runs it in the background and streams its output
// to the terminal panel.

struct Translation {
    // False if the document can't be run; `errors` or `diagnostics` say why.
    bool succeeded = true;
    // A complete C++ program using the mpp runtime (runtime/include/mpp).
    std::string cpp;
    // Problems that aren't about a place in the document.
    std::vector<std::string> errors;
    // Problems in the document, such as an undefined name.
    std::vector<lang::Diagnostic> diagnostics;
    // Where the program can stop with an error; see ParseRuntimeError.
    std::vector<lang::SourceRef> locations;
};

Translation TranslateFile(const FileBase& file);

// A runtime error as the program prints it: "error:LOCATION: message", where
// LOCATION indexes Translation::locations (or is -1 if unknown).
struct RuntimeError {
    int location = -1;
    std::string message;
};
std::optional<RuntimeError> ParseRuntimeError(std::string_view line);

// "Row 3: message", for the terminal.
std::string DescribeAt(const lang::SourceRef& ref, const std::string& message);
