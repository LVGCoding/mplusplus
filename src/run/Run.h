#pragma once

#include <string>
#include <vector>

struct FileBase;

// Running a document: Run -> Run in the menu bar calls RunFile on the active
// file and shows the result in the terminal panel.
//
// This is the place to implement what running does (evaluating the math in a
// .mpp document, plotting graphs, filling tables, ...). The UI only needs
// RunFile and RunResult; everything else can live in src/run/.

struct RunResult {
    // False if the run failed; the output then explains why.
    bool succeeded = true;
    // Lines to print in the terminal, in order.
    std::vector<std::string> output;
};

// Run `file` (a FileMpp for .mpp documents, see editor/FileMpp.h) and return
// what to print. Called after rendering, so it may read the document freely.
RunResult RunFile(const FileBase& file);
