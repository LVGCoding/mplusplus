#include "editor.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <utility>

// Deconstructor, used to cleanup unsaved temp documents at exit
Editor::~Editor() {
    for (const auto& path : temporaryFiles_) {
        std::error_code error;
        std::filesystem::remove(path, error);
        if (error) {
            std::fprintf(stderr, "Could not delete temporary file %s: %s\n",
                         path.string().c_str(), error.message().c_str());
        }
    }
}

// Create a new file object, becoming the active document
void Editor::NewFile(const std::filesystem::path& path) {
    File file;
    file.path = path;
    file.modified = true;

    files_.push_back(std::move(file));
    activeTab_ = files_.size() - 1;
}

// Creates a new temporary file named "untitled.mpp"
bool Editor::NewTemporaryFile(std::string& error) {
    error.clear();
    std::error_code filesystemError;
    const auto temporaryRoot = std::filesystem::temp_directory_path(filesystemError);
    if (filesystemError) {
        error = "Could not find temporary directory: " + filesystemError.message();
        return false;
    }

    // Give each new document its own directory so New never overwrites another file.
    const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    for (int attempt = 0; attempt < 100; ++attempt) {
        const auto directory = temporaryRoot /
            ("mplusplus-" + std::to_string(timestamp) + "-" + std::to_string(attempt));
        if (std::filesystem::create_directory(directory, filesystemError)) {
            File file;
            const auto path = directory / "untitled.mpp";
            temporaryFiles_.push_back(path);
            if (!SaveFile(file, path, error)) {
                return false;
            }
            return OpenFile(path, error);
        }
        if (filesystemError) {
            error = "Could not create temporary directory: " + filesystemError.message();
            return false;
        }
    }

    error = "Could not create a unique temporary directory.";
    return false;
}

// Check if a file is marked as temporary
bool Editor::IsTemporaryFile(const File& file) const {
    return std::find(temporaryFiles_.begin(), temporaryFiles_.end(), file.path)
        != temporaryFiles_.end();
}

// Open a file given a path
bool Editor::OpenFile(const std::filesystem::path& path,
                      std::string& error) {
    File file;

    if (!LoadFile(file, path, error)) {
        return false;
    }

    files_.push_back(std::move(file));
    activeTab_ = files_.size() - 1;
    return true;
}

// Returns the current active file
File* Editor::ActiveFile() {
    if (files_.empty()) {
        return nullptr;
    }

    return &files_[activeTab_];
}
