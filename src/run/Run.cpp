#include "Run.h"

#include "editor/FileMpp.h"
#include "editor/fileBase.h"

Translation TranslateFile(const FileBase& file) {
    Translation result;
    const auto* document = dynamic_cast<const FileMpp*>(&file);
    if (!document) {
        result.succeeded = false;
        result.errors.push_back("Only M++ (.mpp) documents can be run.");
        return result;
    }

    // Phase 0 of docs/language-plan.md: no language yet, just a program that
    // proves the compile-and-run pipeline works end to end.
    // TODO(phase 1): lower document->content, parse, check and generate code.
    const std::size_t rows = document->content.size();
    result.cpp = "#include <mpp/mpp.hpp>\n"
                 "\n"
                 "int main() {\n"
                 "    return mpp::run([] {\n"
                 "        mpp::print(\"Hello from M++! This document has \", std::int64_t{" +
                 std::to_string(rows) + "}, \" rows.\");\n" +
                 "        mpp::print(\"The math in it can't be run yet.\");\n"
                 "    });\n"
                 "}\n";
    return result;
}
