#pragma once

#include "file.h"

#include <cstddef>
#include <vector>

class Editor {
public:
    void NewFile(const std::filesystem::path& path = {});

    bool OpenFile(const std::filesystem::path& path,
                  std::string& error);

    File* ActiveFile();

    const std::vector<File>& Files() const {
        return files_;
    }

private:
    std::vector<File> files_;
    std::size_t activeTab_ = 0;
};