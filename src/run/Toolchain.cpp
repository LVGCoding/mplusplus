#include "Toolchain.h"

#include "Process.h"

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <sstream>

#ifdef __linux__
#include <unistd.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace fs = std::filesystem;

namespace run {
namespace {
fs::path ExecutableDir() {
#ifdef __linux__
    std::error_code error;
    const fs::path self = fs::read_symlink("/proc/self/exe", error);
    return error ? fs::path() : self.parent_path();
#elif defined(__APPLE__)
    char buffer[4096];
    uint32_t size = sizeof buffer;
    return _NSGetExecutablePath(buffer, &size) == 0 ? fs::path(buffer).parent_path() : fs::path();
#else
    return {};
#endif
}

bool HasRuntime(const fs::path& dir) {
    std::error_code error;
    return !dir.empty() && fs::exists(dir / "mpp" / "mpp.hpp", error);
}

fs::path CacheRoot() {
    if (const char* xdg = std::getenv("XDG_CACHE_HOME"); xdg && *xdg) {
        return fs::path(xdg) / "mplusplus";
    }
    if (const char* home = std::getenv("HOME"); home && *home) {
#ifdef __APPLE__
        return fs::path(home) / "Library" / "Caches" / "mplusplus";
#else
        return fs::path(home) / ".cache" / "mplusplus";
#endif
    }
    return fs::temp_directory_path() / "mplusplus-cache";
}

// FNV-1a: a hash that stays the same between runs, for cache folder names.
void Hash(std::uint64_t& hash, std::string_view text) {
    for (const unsigned char c : text) {
        hash = (hash ^ c) * 1099511628211ull;
    }
}

std::string ReadFile(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}

std::optional<std::string> VersionOf(const std::string& compiler) {
    std::vector<Process::Line> lines;
    Process::ExitStatus status;
    std::string error;
    if (!RunAndCollect({compiler, "--version"}, lines, status, error) || !status.Succeeded() ||
        lines.empty()) {
        return std::nullopt;
    }
    return lines.front().text;
}
}  // namespace

std::vector<std::string> Toolchain::CommonFlags() const {
    return {"-std=c++20", "-O1", "-I" + includeDir.string()};
}

fs::path Toolchain::PchHeader() const {
    // One folder per compiler, flags and runtime version, so changing any of
    // them builds a new precompiled header instead of using a stale one.
    std::uint64_t hash = 14695981039346656037ull;
    Hash(hash, compiler);
    Hash(hash, version);
    for (const auto& flag : CommonFlags()) {
        Hash(hash, flag);
    }
    std::error_code error;
    for (const auto& entry : fs::directory_iterator(includeDir / "mpp", error)) {
        Hash(hash, entry.path().filename().string());
        Hash(hash, ReadFile(entry.path()));
    }
    std::ostringstream name;
    name << std::hex << hash;
    return cacheDir / "pch" / name.str() / "mpp_pch.hpp";
}

fs::path Toolchain::PchFile() const {
    fs::path file = PchHeader();
    file += isClang ? ".pch" : ".gch";
    return file;
}

std::vector<std::string> Toolchain::PchCommand() const {
    std::vector<std::string> command = {compiler};
    for (auto& flag : CommonFlags()) {
        command.push_back(flag);
    }
    command.insert(command.end(),
                   {"-x", "c++-header", PchHeader().string(), "-o", PchFile().string()});
    return command;
}

std::vector<std::string> Toolchain::CompileCommand(const fs::path& source, const fs::path& output,
                                                   bool usePch) const {
    std::vector<std::string> command = {compiler};
    for (auto& flag : CommonFlags()) {
        command.push_back(flag);
    }
    if (usePch) {
        // GCC finds mpp_pch.hpp.gch next to the header given to -include;
        // clang needs the .pch named explicitly.
        if (isClang) {
            command.insert(command.end(), {"-include-pch", PchFile().string()});
        } else {
            command.insert(command.end(), {"-include", PchHeader().string()});
        }
    }
    command.insert(command.end(), {source.string(), "-o", output.string()});
    return command;
}

fs::path FindRuntimeIncludeDir() {
    if (const char* dir = std::getenv("MPP_RUNTIME_INCLUDE_DIR"); dir && HasRuntime(dir)) {
        return dir;
    }
    if (const fs::path beside = ExecutableDir() / "runtime" / "include"; HasRuntime(beside)) {
        return beside;
    }
#ifdef MPP_RUNTIME_INCLUDE_DIR
    if (HasRuntime(MPP_RUNTIME_INCLUDE_DIR)) {
        return MPP_RUNTIME_INCLUDE_DIR;
    }
#endif
    return {};
}

std::optional<Toolchain> FindToolchain(std::string& error) {
    Toolchain toolchain;
    toolchain.includeDir = FindRuntimeIncludeDir();
    if (toolchain.includeDir.empty()) {
        error = "Couldn't find the M++ runtime (runtime/include). Try rebuilding M++.";
        return std::nullopt;
    }

    std::vector<std::string> candidates;
    if (const char* cxx = std::getenv("CXX"); cxx && *cxx) {
        candidates.push_back(cxx);
    }
    candidates.insert(candidates.end(), {"c++", "clang++", "g++"});
    for (const auto& candidate : candidates) {
        if (auto version = VersionOf(candidate)) {
            toolchain.compiler = candidate;
            toolchain.version = *version;
            break;
        }
    }
    if (toolchain.compiler.empty()) {
        error = "Couldn't find a C++ compiler. Install g++ or clang++, or set CXX.";
        return std::nullopt;
    }
    toolchain.isClang = toolchain.version.find("clang") != std::string::npos;
    toolchain.cacheDir = CacheRoot();
    return toolchain;
}

}  // namespace run
