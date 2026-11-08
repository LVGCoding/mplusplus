#pragma once

#include "file.h"

#include <cstddef>
#include <vector>

class Editor {
public:
    ~Editor();

    void NewFile(const std::filesystem::path& path = {});

    bool NewTemporaryFile(std::string& error);

    bool IsTemporaryFile(const FileBase& file) const;

    bool OpenFile(const std::filesystem::path& path,
                  std::string& error);

    FileBase* ActiveFile();

    // Call only after unsaved changes have been handled and rendering is done.
    void CloseActiveFile();

    const std::vector<FileBase>& Files() const {
        return files_;
    }

private:
    std::vector<FileBase> files_;
    std::vector<std::filesystem::path> temporaryFiles_;
    std::size_t activeTab_ = 0;
};
