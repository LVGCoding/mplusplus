#include "FileMpp.h"

#include "fileText.h"
#include "parser/parser.h"

bool FileMpp::Load(const std::filesystem::path &path, std::string &error) {
  if (!ReadWholeFile(path, source, error)) {
    return false;
  }
  content = Parser().ParseText(source);
  cursorPos = {0};
  selectionStart = {0};
  this->path = path;
  modified = false;
  return true;
}

bool FileMpp::Save(const std::filesystem::path &path, std::string &error) {
  if (!WriteWholeFile(path, source, error)) {
    return false;
  }
  this->path = path;
  modified = false;
  return true;
}
