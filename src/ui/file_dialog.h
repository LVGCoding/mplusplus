#pragma once

#include <filesystem>
#include <optional>
#include <string>

// Cancellation returns no path and an empty error string.
std::optional<std::filesystem::path> ShowOpenFileDialog(std::string& error);
