#include "editor/FileMpp.h"
#include "editor/fileBase.h"
#include "editor/fileText.h"
#include "test_helpers.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace {
// Creates a fresh temporary directory per test and deletes it afterwards.
class FileTest : public ::testing::Test {
protected:
    std::filesystem::path dir;

    void SetUp() override {
        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        dir = std::filesystem::temp_directory_path() /
              (std::string("mplusplus-test-") + info->name());
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir);
    }

    void TearDown() override {
        std::filesystem::remove_all(dir);
    }

    std::filesystem::path Write(const std::string& name, const std::string& contents) {
        const auto path = dir / name;
        std::ofstream(path, std::ios::binary) << contents;
        return path;
    }
};
}  // namespace

TEST(FileFactory, MppExtensionCreatesFileMpp) {
    EXPECT_NE(dynamic_cast<FileMpp*>(CreateFileForPath("doc.mpp").get()), nullptr);
    EXPECT_NE(dynamic_cast<FileMpp*>(CreateFileForPath("DOC.MPP").get()), nullptr);
}

TEST(FileFactory, OtherExtensionsCreateFileText) {
    for (const char* path : {"notes.txt", "main.cpp", "no_extension", "archive.mpp.bak"}) {
        EXPECT_NE(dynamic_cast<FileText*>(CreateFileForPath(path).get()), nullptr) << path;
    }
}

TEST_F(FileTest, TextFileLoadsAndSaves) {
    const auto path = Write("a.txt", "hello\nworld");
    FileText file;
    std::string error;
    ASSERT_TRUE(file.Load(path, error)) << error;
    EXPECT_EQ(file.text, "hello\nworld");
    EXPECT_FALSE(file.modified);

    file.text += "!";
    file.modified = true;
    ASSERT_TRUE(file.Save(path, error)) << error;
    EXPECT_FALSE(file.modified);

    FileText reloaded;
    ASSERT_TRUE(reloaded.Load(path, error)) << error;
    EXPECT_EQ(reloaded.text, "hello\nworld!");
}

TEST_F(FileTest, MppFileIsParsedOnLoad) {
    const auto path = Write("b.mpp", "\\frac{1}{\\alpha}");
    FileMpp file;
    std::string error;
    ASSERT_TRUE(file.Load(path, error)) << error;
    EXPECT_EQ(file.source, "\\frac{1}{\\alpha}");
    // One row, padded so every cursor position is in a TextNode.
    EXPECT_EQ(test::DescribeList(file.content),
              R"(line(text(""), frac(line(text("1")), line(text(""), greek(alpha), text(""))), text("")))");
    EXPECT_EQ(file.ContentText(), "\\frac{1}{\\alpha}");
}

TEST_F(FileTest, LoadMissingFileFails) {
    FileText file;
    std::string error;
    EXPECT_FALSE(file.Load(dir / "missing.txt", error));
    EXPECT_FALSE(error.empty());
}
