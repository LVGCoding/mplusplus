#pragma once

#include <string>
#include <utility>
#include <vector>

namespace run {

// A child process whose stdout and stderr are read line by line, used to run
// the compiler and compiled M++ programs. The process and anything it starts
// (e.g. the compiler's own subprocesses) are killed when this is destroyed.
//
// POSIX only for now; on other platforms Start() fails with an explanation.
class Process {
public:
    struct Line {
        std::string text;
        bool fromStderr = false;
    };

    struct ExitStatus {
        // True for a normal exit; false if killed by a signal.
        bool exited = false;
        int code = 0;
        int signal = 0;
        bool Succeeded() const { return exited && code == 0; }
    };

    Process() = default;
    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;
    ~Process();

    // Start `argv[0]` (a path, or a name looked up on PATH) with `argv`, adding
    // `environment` to this process's environment.
    bool Start(const std::vector<std::string>& argv,
               const std::vector<std::pair<std::string, std::string>>& environment,
               std::string& error);

    // Wait up to `timeoutMs` for output and append the complete lines that
    // arrived. Returns false once the process has closed both outputs (any
    // last unterminated line is appended then).
    bool ReadLines(int timeoutMs, std::vector<Line>& lines);

    // Kill the process and everything it started.
    void Kill();

    // Wait for the process to exit. Call after ReadLines() returned false or
    // after Kill().
    ExitStatus Wait();

private:
    int pid_ = -1;
    int stdout_ = -1;
    int stderr_ = -1;
    std::string pendingOut_;
    std::string pendingErr_;
    bool reaped_ = false;
    ExitStatus status_;

    void CloseOutputs();
};

// Run to completion and collect all its output. Returns false if it couldn't
// be started (`error` says why); check `status` for how it ended.
bool RunAndCollect(const std::vector<std::string>& argv, std::vector<Process::Line>& lines,
                   Process::ExitStatus& status, std::string& error);

}  // namespace run
