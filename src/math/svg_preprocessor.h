#pragma once


#include <filesystem>

class SVGPreprocessor {
public:
    bool Process(const std::filesystem::path& inputPath, const std::filesystem::path& outputPath);
};
