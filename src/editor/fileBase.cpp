#include "fileBase.h"

#include <algorithm>
#include <cctype>

#include "editor/FileMpp.h"
#include "fileText.h"

const std::string &FileBase::TabName() const {
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

bool IsMppPath(const std::filesystem::path &path) {
  std::string extension = path.extension().string();
  std::transform(
      extension.begin(), extension.end(), extension.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return extension == ".mpp";
}

std::unique_ptr<FileBase> CreateFileForPath(const std::filesystem::path &path) {
  if (IsMppPath(path)) {
    return std::make_unique<FileMpp>();
  }
  return std::make_unique<FileText>();
}
