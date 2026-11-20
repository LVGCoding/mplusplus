#include "Process.h"

#ifdef _WIN32

namespace run {
Process::~Process() = default;
bool Process::Start(const std::vector<std::string>&,
                    const std::vector<std::pair<std::string, std::string>>&, std::string& error) {
    error = "Running programs is not supported on Windows yet.";
    return false;
}
bool Process::ReadLines(int, std::vector<Line>&) { return false; }
void Process::Kill() {}
Process::ExitStatus Process::Wait() { return status_; }
void Process::CloseOutputs() {}
}  // namespace run

#else

#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace run {
namespace {
// Move the complete lines at the front of `pending` into `lines`.
void TakeLines(std::string& pending, bool fromStderr, std::vector<Process::Line>& lines) {
    std::size_t start = 0;
    for (std::size_t end; (end = pending.find('\n', start)) != std::string::npos; start = end + 1) {
        std::size_t length = end - start;
        if (length > 0 && pending[start + length - 1] == '\r') {
            --length;
        }
        lines.push_back({pending.substr(start, length), fromStderr});
    }
    pending.erase(0, start);
}
}  // namespace

Process::~Process() {
    if (pid_ > 0 && !reaped_) {
        Kill();
        Wait();
    }
    CloseOutputs();
}

bool Process::Start(const std::vector<std::string>& argv,
                    const std::vector<std::pair<std::string, std::string>>& environment,
                    std::string& error) {
    if (argv.empty()) {
        error = "nothing to run";
        return false;
    }
    int outPipe[2];
    int errPipe[2];
    if (pipe(outPipe) != 0) {
        error = std::strerror(errno);
        return false;
    }
    if (pipe(errPipe) != 0) {
        error = std::strerror(errno);
        close(outPipe[0]);
        close(outPipe[1]);
        return false;
    }
    // Our ends must not leak into the child (or into later children).
    fcntl(outPipe[0], F_SETFD, FD_CLOEXEC);
    fcntl(errPipe[0], F_SETFD, FD_CLOEXEC);

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, outPipe[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, errPipe[1], STDERR_FILENO);
    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);

    // A process group of its own, so Kill() reaches its children too.
    posix_spawnattr_t attributes;
    posix_spawnattr_init(&attributes);
    posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
    posix_spawnattr_setpgroup(&attributes, 0);

    std::vector<std::string> envStrings;
    for (char** entry = environ; *entry; ++entry) {
        const std::string_view text(*entry);
        bool replaced = false;
        for (const auto& [name, value] : environment) {
            replaced |= text.starts_with(name + "=");
        }
        if (!replaced) {
            envStrings.emplace_back(text);
        }
    }
    for (const auto& [name, value] : environment) {
        envStrings.push_back(name + "=" + value);
    }
    std::vector<char*> envp;
    for (auto& entry : envStrings) {
        envp.push_back(entry.data());
    }
    envp.push_back(nullptr);

    std::vector<std::string> args = argv;
    std::vector<char*> argp;
    for (auto& arg : args) {
        argp.push_back(arg.data());
    }
    argp.push_back(nullptr);

    pid_t pid = -1;
    const int result =
        posix_spawnp(&pid, argp[0], &actions, &attributes, argp.data(), envp.data());
    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attributes);
    close(outPipe[1]);
    close(errPipe[1]);
    if (result != 0) {
        close(outPipe[0]);
        close(errPipe[0]);
        error = "couldn't start " + argv[0] + ": " + std::strerror(result);
        return false;
    }
    pid_ = pid;
    stdout_ = outPipe[0];
    stderr_ = errPipe[0];
    reaped_ = false;
    return true;
}

bool Process::ReadLines(int timeoutMs, std::vector<Line>& lines) {
    if (stdout_ < 0 && stderr_ < 0) {
        return false;
    }
    pollfd fds[2] = {{stdout_, POLLIN, 0}, {stderr_, POLLIN, 0}};
    if (poll(fds, 2, timeoutMs) <= 0) {
        return true;
    }
    for (int i = 0; i < 2; ++i) {
        int& fd = i == 0 ? stdout_ : stderr_;
        std::string& pending = i == 0 ? pendingOut_ : pendingErr_;
        if (fd < 0 || !(fds[i].revents & (POLLIN | POLLHUP | POLLERR))) {
            continue;
        }
        char buffer[4096];
        const ssize_t count = read(fd, buffer, sizeof buffer);
        if (count > 0) {
            pending.append(buffer, static_cast<std::size_t>(count));
            TakeLines(pending, i == 1, lines);
        } else if (count == 0 || (errno != EINTR && errno != EAGAIN)) {
            close(fd);
            fd = -1;
            if (!pending.empty()) {
                lines.push_back({pending, i == 1});
                pending.clear();
            }
        }
    }
    return stdout_ >= 0 || stderr_ >= 0;
}

void Process::Kill() {
    if (pid_ > 0 && !reaped_) {
        ::kill(-pid_, SIGKILL);
    }
}

Process::ExitStatus Process::Wait() {
    if (pid_ <= 0 || reaped_) {
        return status_;
    }
    int raw = 0;
    while (waitpid(pid_, &raw, 0) < 0 && errno == EINTR) {
    }
    reaped_ = true;
    // Its children may still hold the pipes open; don't wait for them.
    ::kill(-pid_, SIGKILL);
    CloseOutputs();
    if (WIFEXITED(raw)) {
        status_ = {true, WEXITSTATUS(raw), 0};
    } else if (WIFSIGNALED(raw)) {
        status_ = {false, 0, WTERMSIG(raw)};
    }
    return status_;
}

void Process::CloseOutputs() {
    if (stdout_ >= 0) {
        close(stdout_);
        stdout_ = -1;
    }
    if (stderr_ >= 0) {
        close(stderr_);
        stderr_ = -1;
    }
}

}  // namespace run

#endif

namespace run {
bool RunAndCollect(const std::vector<std::string>& argv, std::vector<Process::Line>& lines,
                   Process::ExitStatus& status, std::string& error) {
    Process process;
    if (!process.Start(argv, {}, error)) {
        return false;
    }
    while (process.ReadLines(1000, lines)) {
    }
    status = process.Wait();
    return true;
}
}  // namespace run
