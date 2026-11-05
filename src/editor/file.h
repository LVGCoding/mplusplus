#pragma once

#include <filesystem>
#include <string>

// The file class represents a file currently open in the M++ editor.
struct File {
    std::string text;
    std::filesystem::path path;
    bool modified = false;

    const std::string& TabName() const;

private:
    // Keep the label alive until Clay finishes rendering the frame.
    mutable std::string tabName_;
};

bool LoadFile(File& file,
              const std::filesystem::path& path,
              std::string& error);


bool SaveFile(File& file,
              const std::filesystem::path& path,
              std::string& error);
