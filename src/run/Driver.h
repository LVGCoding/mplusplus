#pragma once

#include "lang/Source.h"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace run {

// A line for the terminal panel.
struct OutputLine {
    enum class Kind {
        Info,    // from M++ itself: "Compiling...", "Finished in 0.2 s"
        Output,  // printed by the program
        Error,   // the program's stderr, or a failure
    };
    Kind kind = Kind::Info;
    std::string text;
    // The place in the document an error is about; clicking the line goes there.
    lang::SourceRef source;
};

// Compiles and runs one C++ program on a background thread: builds the
// runtime's precompiled header if needed, compiles, runs the executable, and
// collects its output as it is printed. The UI polls TakeOutput() each frame.
class Job {
public:
    struct Options {
        // Kill the program if it runs longer than this (not counting compiling).
        double timeLimitSeconds = 30.0;
    };

    Job() = default;
    Job(const Job&) = delete;
    Job& operator=(const Job&) = delete;
    // Stops a run in progress.
    ~Job();

    // Start compiling and running `cppSource`. Stops a previous run first.
    void Start(std::string cppSource, Options options);
    void Start(std::string cppSource) { Start(std::move(cppSource), Options{}); }

    // True from Start() until the program has finished and its output is all queued.
    bool Running() const { return running_; }

    // Kill the compiler or program; Running() turns false shortly after.
    void Stop();

    // Wait for the run to finish (for tests).
    void Wait();

    // The lines produced since the last call.
    std::vector<OutputLine> TakeOutput();

    // The results file's lines (see Results.h), once the run has ended; each
    // run's results are taken once.
    std::vector<std::string> TakeResults();

private:
    std::thread thread_;
    std::atomic<bool> running_ = false;
    std::atomic<bool> stopRequested_ = false;
    std::mutex mutex_;
    std::vector<OutputLine> output_;
    std::vector<std::string> results_;

    void Work(std::string cppSource, Options options);
    void Emit(OutputLine::Kind kind, std::string text);
};

}  // namespace run
