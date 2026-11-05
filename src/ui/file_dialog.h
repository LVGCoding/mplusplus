#pragma once

#include <filesystem>
#include <optional>
#include <string>

// Cancellation returns no path and an empty error string.
std::optional<std::filesystem::path> ShowOpenFileDialog(std::string& error);

std::optional<std::filesystem::path> ShowSaveFileDialog(
    const std::filesystem::path& defaultName, std::string& error);
