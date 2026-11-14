#pragma once

#include <filesystem>
#include <optional>
#include <string>

// Both dialogs return no path on cancellation or failure. Cancellation leaves
// `error` empty; failures supply a message. Successful results own their path data.
std::optional<std::filesystem::path> ShowOpenFileDialog(std::string& error);

std::optional<std::filesystem::path> ShowSaveFileDialog(const std::filesystem::path& defaultName,
                                                        std::string& error);
