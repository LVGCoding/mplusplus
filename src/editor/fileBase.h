#pragma once

#include <filesystem>
#include <memory>
#include <string>

// The file base class for representing a file currently open in the M++ editor.
// FileText (plain text) and FileMpp (.mpp math documents) derive from this.
struct FileBase {
    std::filesystem::path path;
    bool modified = false;

    virtual ~FileBase() = default;

    // Read the file at `path` into this object. On success, `this->path` is set.
    virtual bool Load(const std::filesystem::path& path, std::string& error) = 0;

    // Write this object to `path`. On success, `this->path` is set.
    virtual bool Save(const std::filesystem::path& path, std::string& error) = 0;

    virtual const std::string& TabName() const;

protected:
    // Keep the label alive until Clay finishes rendering the frame.
    mutable std::string tabName_;
};

// True if `path` should be opened as an M++ (.mpp) document.
bool IsMppPath(const std::filesystem::path& path);

// Create an empty file object of the right type for `path`:
// FileMpp for .mpp files, FileText for everything else.
std::unique_ptr<FileBase> CreateFileForPath(const std::filesystem::path& path);
