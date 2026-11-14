#include "fileText.h"

#include <algorithm>
#include <fstream>
#include <utility>

bool ReadWholeFile(const std::filesystem::path &path, std::string &text,
                   std::string &error) {
  error.clear();

  std::ifstream input(path, std::ios::binary);
  if (!input.is_open()) {
    error = "Could not open file: " + path.string();
    return false;
  }

  // Read in buffers
  std::string result;
  char buffer[4096];

  while (input.read(buffer, sizeof(buffer))) {
    result.append(buffer, sizeof(buffer));
  }
  result.append(buffer, static_cast<std::size_t>(input.gcount()));

  if (input.bad() || !input.eof()) {
    error = "Could not read file: " + path.string();
    return false;
  }

  text = std::move(result);
  return true;
}

bool WriteWholeFile(const std::filesystem::path &path, const std::string &text,
                    std::string &error) {
  error.clear();

  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output.is_open()) {
    error = "Could not open file for saving: " + path.string();
    return false;
  }

  output.write(text.data(), static_cast<std::streamsize>(text.size()));
  output.close();

  if (!output) {
    error = "Could not finish saving file: " + path.string();
    return false;
  }
  return true;
}

bool FileText::Load(const std::filesystem::path &path, std::string &error) {
  std::string loaded;
  if (!ReadWholeFile(path, loaded, error)) {
    return false;
  }
  savedText_ = loaded;
  Reset(std::move(loaded));
  this->path = path;
  modified = false;
  return true;
}

bool FileText::Save(const std::filesystem::path &path, std::string &error) {
  if (!WriteWholeFile(path, text, error)) {
    return false;
  }
  savedText_ = text;
  this->path = path;
  modified = false;
  return true;
}

void FileText::TextChanged() {
  modified = !savedText_ || text != *savedText_;
}
