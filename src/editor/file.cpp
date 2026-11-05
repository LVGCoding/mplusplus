#include "file.h"

#include <fstream>
#include <utility>

const std::string& File::TabName() const {
    tabName_ = path.filename().string();
    if (tabName_.empty()) {
        tabName_ = "untitled.mpp";
    }

    // Count UTF-8 characters so truncation never splits a multibyte character.
    std::size_t characters = 0;
    std::size_t ellipsisOffset = 0;
    for (std::size_t i = 0; i < tabName_.size(); ++i) {
        if ((static_cast<unsigned char>(tabName_[i]) & 0xC0) == 0x80) {
            continue;
        }
        if (characters == 17) {
            ellipsisOffset = i;
        }
        if (++characters > 20) {
            tabName_.resize(ellipsisOffset);
            tabName_ += "...";
            break;
        }
    }
    return tabName_;
}

bool LoadFile(File& file,
              const std::filesystem::path& path,
              std::string& error) {
    error.clear();

    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        error = "Could not open file: " + path.string();
        return false;
    }

    // Read in buffers
    std::string text;
    char buffer[4096];

    while (input.read(buffer, sizeof(buffer))) {
        text.append(buffer, sizeof(buffer));
    }
    text.append(buffer, static_cast<std::size_t>(input.gcount()));

    if (input.bad() || !input.eof()) {
        error = "Could not read file: " + path.string();
        return false;
    }

    file.text = std::move(text);
    file.path = path;
    file.modified = false;
    return true;
}

bool SaveFile(File& file,
              const std::filesystem::path& path,
              std::string& error) {
    error.clear();

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        error = "Could not open file for saving: " + path.string();
        return false;
    }

    output.write(file.text.data(),
                 static_cast<std::streamsize>(file.text.size()));
    output.close();

    if (!output) {
        error = "Could not finish saving file: " + path.string();
        return false;
    }

    file.path = path;
    file.modified = false;
    return true;
}
