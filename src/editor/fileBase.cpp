#include "fileBase.h"

#include "editor/FileMpp.h"
#include "fileText.h"

#include <algorithm>
#include <cctype>

bool Tab::CopyToClipboard() const {
    const std::string selected = SelectedText();
    if (selected.empty()) {
        return false;
    }
    SetClipboardText(selected.c_str());
    return true;
}

bool Tab::CutToClipboard() {
    return CopyToClipboard() && DeleteSelection();
}

bool Tab::PasteFromClipboard() {
    const char* pasted = GetClipboardText();
    return pasted && PasteText(pasted);
}

const std::string& FileBase::TabName() const {
    tabName_ = path.filename().string();
    if (tabName_.empty()) {
        tabName_ = "untitled.mpp";
    }

    constexpr std::size_t MAX_TAB_CHARACTERS = 20;
    constexpr std::size_t ELLIPSIS_CHARACTERS = 3;

    // Count UTF-8 characters so truncation never splits a multibyte character.
    std::size_t characters = 0;
    std::size_t ellipsisOffset = 0;
    for (std::size_t i = 0; i < tabName_.size(); ++i) {
        if ((static_cast<unsigned char>(tabName_[i]) & 0xC0) == 0x80) {
            continue;
        }
        if (characters == MAX_TAB_CHARACTERS - ELLIPSIS_CHARACTERS) {
            ellipsisOffset = i;
        }
        if (++characters > MAX_TAB_CHARACTERS) {
            tabName_.resize(ellipsisOffset);
            tabName_ += "...";
            break;
        }
    }
    return tabName_;
}

bool IsMppPath(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::transform(
        extension.begin(), extension.end(), extension.begin(), [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    return extension == ".mpp";
}

std::unique_ptr<FileBase> CreateFileForPath(const std::filesystem::path& path) {
    if (IsMppPath(path)) {
        return std::make_unique<FileMpp>();
    }
    return std::make_unique<FileText>();
}
