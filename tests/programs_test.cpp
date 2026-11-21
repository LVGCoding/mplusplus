// Golden programs: each tests/programs/NAME.mpp is compiled and run, and its
// output must match NAME.out. Runtime errors appear in the output as
// "! Row N: message". Skipped when no C++ compiler is installed.

#include "editor/FileMpp.h"
#include "run/Driver.h"
#include "run/Run.h"
#include "run/Toolchain.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

std::filesystem::path ProgramsDir() {
    return std::filesystem::path(MPP_SOURCE_DIR) / "tests" / "programs";
}

std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}

// Compile and run a document; its output with errors as "! Row N: message".
std::string RunDocument(const std::filesystem::path& path) {
    FileMpp file;
    std::string error;
    EXPECT_TRUE(file.Load(path, error)) << error;
    Translation translation = TranslateFile(file);
    if (!translation.succeeded) {
        std::string text;
        for (const auto& diagnostic : translation.diagnostics) {
            text += "compile error: " + DescribeAt(diagnostic.ref, diagnostic.message) + "\n";
        }
        return text;
    }
    run::Job job;
    job.Start(translation.cpp);
    job.Wait();
    std::string text;
    for (const auto& line : job.TakeOutput()) {
        if (line.kind == run::OutputLine::Kind::Output) {
            text += line.text + "\n";
        } else if (auto runtimeError = ParseRuntimeError(line.text)) {
            const int location = runtimeError->location;
            const bool known =
                location >= 0 && static_cast<std::size_t>(location) < translation.locations.size();
            text += "! " +
                    (known ? DescribeAt(translation.locations[location], runtimeError->message)
                           : runtimeError->message) +
                    "\n";
        } else if (line.kind == run::OutputLine::Kind::Error &&
                   line.text.find("Internal error") != std::string::npos) {
            text += line.text + "\n";
        }
    }
    return text;
}

class Programs : public ::testing::TestWithParam<std::string> {};

}  // namespace

TEST_P(Programs, ProduceTheExpectedOutput) {
    std::string error;
    if (!run::FindToolchain(error)) {
        GTEST_SKIP() << error;
    }
    const std::filesystem::path source = ProgramsDir() / (GetParam() + ".mpp");
    EXPECT_EQ(RunDocument(source), ReadFile(ProgramsDir() / (GetParam() + ".out")));
}

// The help document (Docs menu) is runnable: its prose is % comments and
// every example in it must compile and run.
TEST(HelpDocument, RunsWithoutErrors) {
    std::string error;
    if (!run::FindToolchain(error)) {
        GTEST_SKIP() << error;
    }
    const std::string output =
        RunDocument(std::filesystem::path(MPP_SOURCE_DIR) / "assets" / "doc" / "help.mpp");
    EXPECT_EQ(output.find("compile error"), std::string::npos) << output;
    // Runtime errors are lines starting with "! ".
    EXPECT_EQ(("\n" + output).find("\n! "), std::string::npos) << output;
    EXPECT_NE(output.find("Hello Ada!"), std::string::npos) << output;
}

INSTANTIATE_TEST_SUITE_P(All, Programs, ::testing::ValuesIn([] {
                             std::vector<std::string> names;
                             for (const auto& entry :
                                  std::filesystem::directory_iterator(ProgramsDir())) {
                                 if (entry.path().extension() == ".mpp") {
                                     names.push_back(entry.path().stem().string());
                                 }
                             }
                             std::sort(names.begin(), names.end());
                             return names;
                         }()),
                         [](const auto& info) { return info.param; });
