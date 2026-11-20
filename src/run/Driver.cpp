#include "Driver.h"

#include "Process.h"
#include "Toolchain.h"

#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <utility>

#ifndef _WIN32
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace run {
namespace {
using Clock = std::chrono::steady_clock;

// Most compiler errors are useless past the first screen.
constexpr std::size_t maxCompilerLines = 40;

std::string Seconds(Clock::duration duration) {
    char text[32];
    std::snprintf(text, sizeof text, "%.2f s",
                  std::chrono::duration<double>(duration).count());
    return text;
}

int ProcessId() {
#ifdef _WIN32
    return 0;
#else
    return static_cast<int>(getpid());
#endif
}

// A folder for this editor's runs, so two open editors don't collide.
fs::path RunDir(const Toolchain& toolchain) {
    return toolchain.cacheDir / "runs" / std::to_string(ProcessId());
}

bool WriteFile(const fs::path& path, const std::string& text) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << text;
    return static_cast<bool>(file);
}
}  // namespace

Job::~Job() {
    Stop();
    if (thread_.joinable()) {
        thread_.join();
    }
}

void Job::Start(std::string cppSource, Options options) {
    Stop();
    if (thread_.joinable()) {
        thread_.join();
    }
    stopRequested_ = false;
    running_ = true;
    thread_ = std::thread(&Job::Work, this, std::move(cppSource), options);
}

void Job::Stop() {
    if (running_) {
        stopRequested_ = true;
    }
}

void Job::Wait() {
    if (thread_.joinable()) {
        thread_.join();
    }
}

std::vector<std::string> Job::TakeResults() {
    std::lock_guard lock(mutex_);
    return std::exchange(results_, {});
}

std::vector<OutputLine> Job::TakeOutput() {
    std::lock_guard lock(mutex_);
    return std::exchange(output_, {});
}

void Job::Emit(OutputLine::Kind kind, std::string text) {
    std::lock_guard lock(mutex_);
    output_.push_back({kind, std::move(text)});
}

void Job::Work(std::string cppSource, Options options) {
    using Kind = OutputLine::Kind;
    // Runs `command` to the end, or until Stop() / the deadline. Each line
    // goes to `onLine`. Returns how it ended, or nullopt if it didn't start.
    enum class Ending { Finished, Stopped, TimedOut, NotStarted };
    auto runCommand = [&](const std::vector<std::string>& command,
                          const std::vector<std::pair<std::string, std::string>>& environment,
                          std::optional<Clock::time_point> deadline, auto&& onLine,
                          Process::ExitStatus& status) {
        Process process;
        std::string error;
        if (!process.Start(command, environment, error)) {
            Emit(Kind::Error, error);
            return Ending::NotStarted;
        }
        std::vector<Process::Line> lines;
        Ending ending = Ending::Finished;
        while (process.ReadLines(50, lines)) {
            for (auto& line : lines) {
                onLine(line);
            }
            lines.clear();
            if (stopRequested_) {
                ending = Ending::Stopped;
            } else if (deadline && Clock::now() > *deadline) {
                ending = Ending::TimedOut;
            }
            if (ending != Ending::Finished) {
                process.Kill();
                break;
            }
        }
        for (auto& line : lines) {
            onLine(line);
        }
        status = process.Wait();
        return ending;
    };

    auto finish = [&] { running_ = false; };

    std::string error;
    const auto toolchain = FindToolchain(error);
    if (!toolchain) {
        Emit(Kind::Error, error);
        return finish();
    }

    std::error_code fsError;
    const fs::path runDir = RunDir(*toolchain);
    fs::create_directories(runDir, fsError);
    const fs::path source = runDir / "program.cpp";
    const fs::path executable = runDir / "program";
    const fs::path results = runDir / "results.jsonl";
    if (!WriteFile(source, cppSource)) {
        Emit(Kind::Error, "Couldn't write " + source.string());
        return finish();
    }

    // The runtime header (Eigen, Boost) takes seconds to compile; compile it
    // once and reuse it. If that fails, compile without it.
    bool usePch = fs::exists(toolchain->PchFile(), fsError);
    if (!usePch) {
        Emit(Kind::Info, "Preparing the math libraries (only needed once)...");
        fs::create_directories(toolchain->PchHeader().parent_path(), fsError);
        WriteFile(toolchain->PchHeader(), "#include <mpp/mpp.hpp>\n");
        const fs::path partial = toolchain->PchFile().string() + ".partial";
        auto command = toolchain->PchCommand();
        command.back() = partial.string();
        std::vector<std::string> errors;
        Process::ExitStatus status;
        const Ending ending = runCommand(
            command, {}, std::nullopt,
            [&](const Process::Line& line) { errors.push_back(line.text); }, status);
        if (ending == Ending::Stopped) {
            fs::remove(partial, fsError);
            Emit(Kind::Info, "Stopped.");
            return finish();
        }
        if (ending == Ending::Finished && status.Succeeded()) {
            // Renamed into place only when complete, so a stopped build
            // never leaves a broken header behind.
            fs::rename(partial, toolchain->PchFile(), fsError);
            usePch = !fsError;
        } else {
            fs::remove(partial, fsError);
            Emit(Kind::Info, "Couldn't precompile the math libraries; runs will be slower.");
            for (std::size_t i = 0; i < errors.size() && i < maxCompilerLines; ++i) {
                Emit(Kind::Info, errors[i]);
            }
        }
    }

    Emit(Kind::Info, "Compiling...");
    const auto compileStart = Clock::now();
    std::vector<std::string> compilerOutput;
    Process::ExitStatus compileStatus;
    const Ending compiled = runCommand(
        toolchain->CompileCommand(source, executable, usePch), {}, std::nullopt,
        [&](const Process::Line& line) { compilerOutput.push_back(line.text); }, compileStatus);
    if (compiled == Ending::NotStarted) {
        return finish();
    }
    if (compiled == Ending::Stopped) {
        Emit(Kind::Info, "Stopped.");
        return finish();
    }
    if (!compileStatus.Succeeded()) {
        // The checker should catch every mistake in the document first, so
        // this is a bug in M++'s code generation, not the user's fault.
        Emit(Kind::Error, "Internal error: the generated C++ didn't compile. This is a bug in M++.");
        for (std::size_t i = 0; i < compilerOutput.size() && i < maxCompilerLines; ++i) {
            Emit(Kind::Error, compilerOutput[i]);
        }
        if (compilerOutput.size() > maxCompilerLines) {
            Emit(Kind::Error, "...");
        }
        Emit(Kind::Error, "The generated program is " + source.string());
        return finish();
    }
    Emit(Kind::Info, "Compiled in " + Seconds(Clock::now() - compileStart) + ".");

    fs::remove(results, fsError);
    const auto runStart = Clock::now();
    const auto deadline =
        runStart + std::chrono::duration_cast<Clock::duration>(
                       std::chrono::duration<double>(options.timeLimitSeconds));
    Process::ExitStatus status;
    const Ending ran = runCommand(
        {executable.string()}, {{"MPP_RESULTS", results.string()}}, deadline,
        [&](const Process::Line& line) {
            Emit(line.fromStderr ? Kind::Error : Kind::Output, line.text);
        },
        status);
    const std::string elapsed = Seconds(Clock::now() - runStart);
    {
        // What the program sent to the editor, even if it stopped early.
        std::ifstream in(results);
        std::vector<std::string> lines;
        for (std::string line; std::getline(in, line);) {
            lines.push_back(std::move(line));
        }
        std::lock_guard lock(mutex_);
        results_ = std::move(lines);
    }
    switch (ran) {
    case Ending::NotStarted:
        break;
    case Ending::Stopped:
        Emit(Kind::Info, "Stopped after " + elapsed + ".");
        break;
    case Ending::TimedOut:
        Emit(Kind::Error, "Stopped: the program ran longer than the time limit (" + elapsed + ").");
        break;
    case Ending::Finished:
        if (status.Succeeded()) {
            Emit(Kind::Info, "Finished in " + elapsed + ".");
        } else if (status.exited) {
            Emit(Kind::Error, "The program stopped with an error.");
        } else {
            const char* name = strsignal(status.signal);
            Emit(Kind::Error, std::string("The program crashed (") +
                                  (name ? name : std::to_string(status.signal)) + ").");
        }
        break;
    }
    finish();
}

}  // namespace run
