#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

// The file base class for representing a file currently open in the M++ editor.
struct FileBase {
    std::filesystem::path path;

    virtual const std::string& TabName() const;
};