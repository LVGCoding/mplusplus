#pragma once

#include "editor/FileMpp.h"
#include "fileBase.h"
#include "fileText.h"

#include <cstddef>
#include <memory>
#include <vector>

class Editor {
public:
  ~Editor();

  void NewFile(const std::filesystem::path &path = {});

  bool NewTemporaryFile(std::string &error);

  bool IsTemporaryFile(const FileBase &file) const;

  // Opens `path` as a FileMpp if it has the .mpp extension, otherwise as a
  // FileText.
  bool OpenFile(const std::filesystem::path &path, std::string &error);

  FileBase *ActiveFile();

  std::size_t ActiveTabIndex() const { return activeTab_; }

  // Make the file at `index` the active tab. Out-of-range indices are ignored.
  void SetActiveTab(std::size_t index);

  // Call only after unsaved changes have been handled and rendering is done.
  void CloseActiveFile();

  const std::vector<std::unique_ptr<FileBase>> &Files() const { return files_; }

private:
  // Held by pointer so FileText/FileMpp aren't sliced down to FileBase.
  std::vector<std::unique_ptr<FileBase>> files_;
  std::vector<std::filesystem::path> temporaryFiles_;
  std::size_t activeTab_ = 0;
};
