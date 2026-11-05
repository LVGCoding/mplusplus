#pragma once

#include "file.h"

#include <cstddef>
#include <vector>

class Editor {
public:
    ~Editor();

    void NewFile(const std::filesystem::path& path = {});

    bool NewTemporaryFile(std::string& error);

    bool IsTemporaryFile(const File& file) const;

    bool OpenFile(const std::filesystem::path& path,
                  std::string& error);

    File* ActiveFile();

    // Call only after unsaved changes have been handled and rendering is done.
    void CloseActiveFile();

    const std::vector<File>& Files() const {
        return files_;
    }

private:
    std::vector<File> files_;
    std::vector<std::filesystem::path> temporaryFiles_;
    std::size_t activeTab_ = 0;
};
