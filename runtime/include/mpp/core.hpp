#pragma once

// Errors and the entry point of a compiled M++ program.

#include <cstdio>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace mpp {

// The document row (1-based) and column of the statement running now.
// Generated code sets these before each statement, so errors can point at it.
inline int row = 0;
inline int column = 0;

// A runtime error in the user's program, e.g. an index out of range.
struct Error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

[[noreturn]] inline void fail(std::string message) {
    throw Error(std::move(message));
}

// Runs the program's top level. Errors are printed to stderr as
//   error:ROW:COLUMN: message
// which the editor reads to highlight where the program stopped.
template <class Body>
int run(Body body) {
    // Output goes to a pipe; flush each line so the editor shows it as it comes.
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    try {
        body();
        std::cout.flush();
        return 0;
    } catch (const Error& error) {
        std::cout.flush();
        std::cerr << "error:" << row << ':' << column << ": " << error.what() << std::endl;
    } catch (const std::exception& error) {
        std::cout.flush();
        std::cerr << "error:" << row << ':' << column << ": internal error: " << error.what()
                  << std::endl;
    }
    return 1;
}

}  // namespace mpp
