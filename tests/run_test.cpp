// Compiling and running programs with run::Job. These start the real
// compiler, so they are slow the first time (the runtime's precompiled header
// is built once and cached), and skipped when no compiler is installed.

#include "run/Driver.h"
#include "run/Process.h"
#include "run/Toolchain.h"

#include <gtest/gtest.h>

#include <string>
#include <thread>
#include <vector>

namespace {
using Kind = run::OutputLine::Kind;

class RunJobTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::string error;
        if (!run::FindToolchain(error)) {
            GTEST_SKIP() << error;
        }
    }

    // Run `body` as the program's top level and return its output.
    static std::vector<run::OutputLine> RunMain(const std::string& body,
                                                run::Job::Options options = {}) {
        run::Job job;
        job.Start("#include <mpp/mpp.hpp>\nint main() { return mpp::run([] {\n" + body +
                      "\n}); }\n",
                  options);
        job.Wait();
        EXPECT_FALSE(job.Running());
        return job.TakeOutput();
    }

    static std::vector<std::string> Lines(const std::vector<run::OutputLine>& output, Kind kind) {
        std::vector<std::string> lines;
        for (const auto& line : output) {
            if (line.kind == kind) {
                lines.push_back(line.text);
            }
        }
        return lines;
    }

    static bool Contains(const std::vector<run::OutputLine>& output, Kind kind,
                         const std::string& text) {
        for (const auto& line : output) {
            if (line.kind == kind && line.text.find(text) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
};
}  // namespace

TEST_F(RunJobTest, PrintsTheProgramsOutput) {
    const auto output = RunMain(R"(mpp::print("x = ", 0.1 + 0.2); mpp::print(true, " ", std::int64_t{42});)");
    EXPECT_EQ(Lines(output, Kind::Output),
              (std::vector<std::string>{"x = 0.3", "true 42"}));
    EXPECT_TRUE(Contains(output, Kind::Info, "Finished"));
}

TEST_F(RunJobTest, RuntimeLibrariesAreAvailable) {
    const auto output = RunMain(R"(
        Eigen::Matrix2d a{{2, 0}, {0, 4}};
        mpp::print(a.determinant());
        boost::math::normal_distribution<double> n(0, 1);
        mpp::print(boost::math::cdf(n, 0.0));)");
    EXPECT_EQ(Lines(output, Kind::Output), (std::vector<std::string>{"8", "0.5"}));
}

TEST_F(RunJobTest, RuntimeErrorsNameTheirLocation) {
    const auto output = RunMain(R"(mpp::where = 7; mpp::fail("index 4 out of range");)");
    EXPECT_TRUE(Contains(output, Kind::Error, "error:7: index 4 out of range"));
    EXPECT_TRUE(Contains(output, Kind::Error, "stopped with an error"));
}

TEST_F(RunJobTest, CodeThatDoesNotCompileIsReportedAsAnInternalError) {
    const auto output = RunMain("this is not C++;");
    EXPECT_TRUE(Contains(output, Kind::Error, "Internal error"));
    EXPECT_FALSE(Contains(output, Kind::Info, "Finished"));
}

TEST_F(RunJobTest, TimeLimitStopsEndlessPrograms) {
    const auto output = RunMain("for (;;) {}", {.timeLimitSeconds = 0.3});
    EXPECT_TRUE(Contains(output, Kind::Error, "time limit"));
}

TEST_F(RunJobTest, StopKillsARunningProgram) {
    run::Job job;
    job.Start(R"(#include <mpp/mpp.hpp>
                 int main() { return mpp::run([] { mpp::print("started"); for (;;) {} }); })");
    // Wait until it is running, then stop it.
    bool started = false;
    std::vector<run::OutputLine> output;
    for (int i = 0; i < 600 && !started; ++i) {
        for (auto& line : job.TakeOutput()) {
            started |= line.text == "started";
            output.push_back(line);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    ASSERT_TRUE(started);
    job.Stop();
    job.Wait();
    EXPECT_FALSE(job.Running());
    for (auto& line : job.TakeOutput()) {
        output.push_back(line);
    }
    EXPECT_TRUE(Contains(output, Kind::Info, "Stopped"));
}

TEST(Process, CollectsStdoutAndStderrSeparately) {
    std::vector<run::Process::Line> lines;
    run::Process::ExitStatus status;
    std::string error;
    ASSERT_TRUE(run::RunAndCollect({"sh", "-c", "echo out; echo err >&2; printf last"}, lines,
                                   status, error))
        << error;
    EXPECT_TRUE(status.Succeeded());
    ASSERT_EQ(lines.size(), 3u);
    int stderrLines = 0;
    for (const auto& line : lines) {
        stderrLines += line.fromStderr;
    }
    EXPECT_EQ(stderrLines, 1);
}

TEST(Process, ReportsAProgramThatCannotStart) {
    std::vector<run::Process::Line> lines;
    run::Process::ExitStatus status;
    std::string error;
    EXPECT_FALSE(run::RunAndCollect({"/no/such/program"}, lines, status, error));
    EXPECT_FALSE(error.empty());
}
