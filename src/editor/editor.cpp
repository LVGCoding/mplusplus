#include "editor.h"

#include <utility>

void Editor::NewFile(const std::filesystem::path& path) {
    File file;
    file.path = path;
    file.modified = true;

    files_.push_back(std::move(file));
    activeTab_ = files_.size() - 1;
}

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

File* Editor::ActiveFile() {
    if (files_.empty()) {
        return nullptr;
    }

    return &files_[activeTab_];
}