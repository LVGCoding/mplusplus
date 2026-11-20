#include "Run.h"

#include "editor/FileMpp.h"
#include "editor/fileBase.h"
#include "lang/Compile.h"

#include <charconv>

Translation TranslateFile(const FileBase& file) {
    Translation result;
    const auto* document = dynamic_cast<const FileMpp*>(&file);
    if (!document) {
        result.succeeded = false;
        result.errors.push_back("Only M++ (.mpp) documents can be run.");
        return result;
    }
    lang::Compilation compilation = lang::Compile(document->content);
    result.succeeded = compilation.Succeeded();
    result.diagnostics = std::move(compilation.diagnostics);
    result.cpp = std::move(compilation.code.cpp);
    result.locations = std::move(compilation.code.locations);
    return result;
}

std::optional<RuntimeError> ParseRuntimeError(std::string_view line) {
    constexpr std::string_view PREFIX = "error:";
    if (!line.starts_with(PREFIX)) {
        return std::nullopt;
    }
    line.remove_prefix(PREFIX.size());
    RuntimeError error;
    const auto [end, problem] = std::from_chars(line.data(), line.data() + line.size(),
                                                error.location);
    if (problem != std::errc() || end == line.data() + line.size() || *end != ':') {
        return std::nullopt;
    }
    line.remove_prefix(end - line.data() + 1);
    if (line.starts_with(' ')) {
        line.remove_prefix(1);
    }
    error.message = std::string(line);
    return error;
}

std::string DescribeAt(const lang::SourceRef& ref, const std::string& message) {
    if (ref.Empty()) {
        return message;
    }
    return "Row " + std::to_string(ref.Row() + 1) + ": " + message;
}
