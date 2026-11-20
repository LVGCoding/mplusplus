#pragma once

// Errors and the entry point of a compiled M++ program.

#include <cstdio>
#include <pthread.h>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace mpp {

// The statement running now, as an index into the locations the compiler
// recorded (lang::GeneratedCode::locations). Generated code sets it before
// each statement, so any error can point at the statement.
inline int where = -1;

// A runtime error in the user's program, e.g. a division by zero, and where
// it happened (a location index, like `where`).
struct Error : std::runtime_error {
    int location;
    Error(int location, const std::string& message)
        : std::runtime_error(message), location(location) {}
};

// Stop with an error at the statement running now.
[[noreturn]] inline void fail(const std::string& message) {
    throw Error(where, message);
}

// Stop with an error at a specific location, e.g. the denominator of a fraction.
[[noreturn]] inline void fail_at(int location, const std::string& message) {
    throw Error(location, message);
}

// Runs the program's top level. Errors are printed to stderr as
//   error:LOCATION: message
// which the editor reads to underline where the program stopped.
template <class Body>
int run_here(Body& body) {
    // Output goes to a pipe; flush each line so the editor shows it as it comes.
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    try {
        body();
        std::cout.flush();
        return 0;
    } catch (const Error& error) {
        std::cout.flush();
        std::cerr << "error:" << error.location << ": " << error.what() << std::endl;
    } catch (const std::exception& error) {
        std::cout.flush();
        std::cerr << "error:" << where << ": internal error: " << error.what() << std::endl;
    }
    return 1;
}

// Like run_here, on a thread with a big stack, so functions that use
// themselves can go deep (see Call).
template <class Body>
int run(Body body) {
    struct Task {
        Body* body;
        int result;
    } task{&body, 1};
    pthread_attr_t attributes;
    pthread_attr_init(&attributes);
    pthread_attr_setstacksize(&attributes, std::size_t{1} << 30);
    pthread_t thread;
    auto start = [](void* data) -> void* {
        auto* task = static_cast<Task*>(data);
        task->result = run_here(*task->body);
        return nullptr;
    };
    if (pthread_create(&thread, &attributes, start, &task) != 0) {
        return run_here(body);
    }
    pthread_join(thread, nullptr);
    return task.result;
}

// Counts calls of the document's functions inside each other, to stop with
// an error instead of crashing when a function uses itself without end.
inline int depth = 0;
inline constexpr int MAX_DEPTH = 100000;

struct Call {
    Call(int at, const char* name) {
        if (++depth > MAX_DEPTH) {
            --depth;
            fail_at(at, std::string(name) + " uses itself more than " +
                            std::to_string(MAX_DEPTH) +
                            " times inside each other; does it ever stop?");
        }
    }
    ~Call() { --depth; }
    Call(const Call&) = delete;
    Call& operator=(const Call&) = delete;
};

}  // namespace mpp
