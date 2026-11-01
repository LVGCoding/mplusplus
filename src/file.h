#pragma once

#include <filesystem>
#include <string>

// The file class represents a file currently open in the M++ editor.
struct File {
    std::string text;
    std::filesystem::path path;
    bool modified = false;
}

bool LoadFile(File& file,
              const std::filesystem::path& path,
              std::string& error);


bool SaveFile(File& file,
              const std::filesystem::path& path,
              std::string& error);
