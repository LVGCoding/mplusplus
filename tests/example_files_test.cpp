// The documents in example/ load and save back unchanged.

#include "editor/FileMpp.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>

#ifndef MPP_SOURCE_DIR
#define MPP_SOURCE_DIR "."
#endif

TEST(ExampleFiles, RoundTrip) {
    const std::filesystem::path dir = std::filesystem::path(MPP_SOURCE_DIR) / "example";
    int checked = 0;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().extension() != ".mpp") {
            continue;
        }
        std::ifstream file(entry.path(), std::ios::binary);
        std::stringstream text;
        text << file.rdbuf();
        FileMpp document;
        document.SetSource(text.str());
        EXPECT_EQ(document.ContentText(), text.str()) << entry.path();
        ++checked;
    }
    EXPECT_GT(checked, 0);
}
