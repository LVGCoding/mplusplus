#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace run {

// The C++ compiler and runtime headers used to build M++ programs.
struct Toolchain {
    // The compiler, as given or found on PATH.
    std::string compiler;
    // First line of `compiler --version`.
    std::string version;
    bool isClang = false;
    // Holds mpp/, Eigen/ and boost/.
    std::filesystem::path includeDir;
    // Where precompiled headers are kept between runs.
    std::filesystem::path cacheDir;

    // Flags every compile uses; the precompiled header must match them.
    std::vector<std::string> CommonFlags() const;

    // The precompiled runtime header for this compiler and runtime: the
    // header to pass with -include, and the compiled file next to it.
    std::filesystem::path PchHeader() const;
    std::filesystem::path PchFile() const;
    std::vector<std::string> PchCommand() const;

    // Compile `source` to the executable `output`, using the precompiled
    // header when `usePch`.
    std::vector<std::string> CompileCommand(const std::filesystem::path& source,
                                            const std::filesystem::path& output,
                                            bool usePch) const;
};

// The folder with the runtime headers: $MPP_RUNTIME_INCLUDE_DIR, else
// runtime/include next to the executable, else the build folder this was
// compiled in. Empty if none has mpp/mpp.hpp.
std::filesystem::path FindRuntimeIncludeDir();

// $CXX, else the first of c++, clang++, g++ on PATH; checks it runs.
std::optional<Toolchain> FindToolchain(std::string& error);

}  // namespace run
