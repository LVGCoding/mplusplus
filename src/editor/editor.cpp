#include "editor.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <utility>

Editor::~Editor() {
    for (const auto& path : temporaryFiles_) {
        std::error_code error;
        std::filesystem::remove(path, error);
        if (error) {
            std::fprintf(stderr,
                         "Could not delete temporary file %s: %s\n",
                         path.string().c_str(),
                         error.message().c_str());
        }
    }
}

void Editor::NewFile(const std::filesystem::path& path) {
    auto file = CreateFileForPath(path);
    file->path = path;
    file->modified = true;

    files_.push_back(std::move(file));
    activeTab_ = files_.size() - 1;
}

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
    constexpr int MAX_DIRECTORY_ATTEMPTS = 100;
    for (int attempt = 0; attempt < MAX_DIRECTORY_ATTEMPTS; ++attempt) {
        const auto directory = temporaryRoot / ("mplusplus-" + std::to_string(timestamp) + "-" +
                                                std::to_string(attempt));
        if (std::filesystem::create_directory(directory, filesystemError)) {
            const auto path = directory / "untitled.mpp";
            // Track the path before saving so cleanup also handles a partial write.
            temporaryFiles_.push_back(path);
            if (!CreateFileForPath(path)->Save(path, error)) {
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

bool Editor::IsTemporaryFile(const FileBase& file) const {
    return std::find(temporaryFiles_.begin(), temporaryFiles_.end(), file.path) !=
           temporaryFiles_.end();
}

bool Editor::OpenFile(const std::filesystem::path& path,
                      std::string& error) {
    auto file = CreateFileForPath(path);

    if (!file->Load(path, error)) {
        return false;
    }

    // Publish the new tab only after loading succeeds; existing tabs stay untouched on failure.
    files_.push_back(std::move(file));
    activeTab_ = files_.size() - 1;
    return true;
}

FileBase* Editor::ActiveFile() {
    if (files_.empty()) {
        return nullptr;
    }

    return files_[activeTab_].get();
}

void Editor::SetActiveTab(std::size_t index) {
    if (index < files_.size()) {
        activeTab_ = index;
    }
}

void Editor::CloseActiveFile() {
    if (files_.empty()) {
        return;
    }
    files_.erase(files_.begin() + activeTab_);
    if (files_.empty()) {
        activeTab_ = 0;
    } else if (activeTab_ >= files_.size()) {
        activeTab_ = files_.size() - 1;
    }
}
