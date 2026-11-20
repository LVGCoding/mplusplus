#pragma once

// Handlers for the requests recorded while building the layout.
// Called from ProcessLayoutActions after rendering so document mutations cannot
// invalidate text that Clay still needs to draw.

#include "layout_common.h"
#include "layout_state.h"
#include "layout_tabs.h"
#include "editor/FileMpp.h"
#include "editor/GraphTab.h"
#include "run/Results.h"
#include "lang/Compile.h"
#include "run/Run.h"

namespace layout_internal {
inline void ProcessHistoryRequests(Editor& editor) {
    Tab* file = editor.ActiveTab();
    if (undoRequested) {
        undoRequested = false;
        if (file && file->CanUndo()) {
            historyActivity |= file->Undo();
        }
    }
    if (redoRequested) {
        redoRequested = false;
        if (file && file->CanRedo()) {
            historyActivity |= file->Redo();
        }
    }
}

// Edit -> Cut / Copy / Paste.
inline void ProcessClipboardRequests(Editor& editor) {
    Tab* file = editor.ActiveTab();
    if (cutRequested) {
        cutRequested = false;
        if (file) {
            historyActivity |= file->CutToClipboard();
        }
    }
    if (copyRequested) {
        copyRequested = false;
        if (file) {
            file->CopyToClipboard();
        }
    }
    if (pasteRequested) {
        pasteRequested = false;
        if (file) {
            historyActivity |= file->PasteFromClipboard();
        }
    }
}

// The terminal line for a problem in the document; clicking it goes there.
inline run::OutputLine ProblemLine(const lang::Diagnostic& problem) {
    return {run::OutputLine::Kind::Error, DescribeAt(problem.ref, problem.message), problem.ref};
}

// The document the terminal's errors are about, if it is still open and
// unchanged since it was run (so the positions in the errors still fit).
inline FileMpp* TerminalDocument(Editor& editor, bool requireSameVersion) {
    if (FileIndex(editor.Files(), terminalFile) == SIZE_MAX) {
        return nullptr;
    }
    auto* document = dynamic_cast<FileMpp*>(const_cast<Tab*>(terminalFile));
    if (document && requireSameVersion && document->ContentVersion() != terminalFileVersion) {
        return nullptr;
    }
    return document;
}

// A runtime error from the program ("error:7: division by zero") as
// "Row 3: division by zero", pointing at where it happened. The place is
// underlined in the document too, if it hasn't changed since.
inline void AddRuntimeErrorSource(Editor& editor, run::OutputLine& line) {
    const auto error = ParseRuntimeError(line.text);
    if (!error) {
        return;
    }
    if (error->location < 0 || static_cast<std::size_t>(error->location) >= runLocations.size()) {
        line.text = error->message;
        return;
    }
    const lang::SourceRef& ref = runLocations[static_cast<std::size_t>(error->location)];
    line.text = DescribeAt(ref, error->message);
    line.source = ref;
    if (FileMpp* document = TerminalDocument(editor, true)) {
        document->diagnostics.push_back({error->message, ref});
    }
}

// Clicking an error in the terminal: show the document and put the cursor
// at the start of the problem.
inline void ProcessTerminalJump(Editor& editor) {
    const std::size_t index = terminalJumpRequested;
    terminalJumpRequested = SIZE_MAX;
    if (index >= terminalOutput.size() || terminalOutput[index].source.Empty()) {
        return;
    }
    FileMpp* document = TerminalDocument(editor, false);
    if (!document) {
        return;
    }
    SwitchTab(editor, FileIndex(editor.Files(), document));
    const CursorPath& target = terminalOutput[index].source.start;
    if (document->CursorText(target)) {
        document->MoveCursorTo(target);
        historyActivity = true;  // scrolls the cursor into view
    }
}

// Once a document has been run, check it again a moment after each edit,
// so its underlines stay up to date while it is fixed.
inline void CheckWhileEditing(Editor& editor) {
    auto* document = dynamic_cast<FileMpp*>(editor.ActiveTab());
    if (!document || !document->checkWhileEditing) {
        return;
    }
    const std::uint64_t version = document->ContentVersion();
    if (checkedVersions[document] == version) {
        return;
    }
    if (uncheckedFile != document || uncheckedVersion != version) {
        uncheckedFile = document;
        uncheckedVersion = version;
        uncheckedSince = GetTime();
        return;
    }
    constexpr double CHECK_DELAY = 0.4;  // seconds after the last edit
    if (GetTime() - uncheckedSince >= CHECK_DELAY) {
        document->diagnostics =
            lang::Compile(document->content, lang::CompileMode::CheckOnly).diagnostics;
        checkedVersions[document] = version;
    }
}

// Run -> Run: translate the active file (src/run/Run.cpp) and start compiling
// and running it in the background. Run -> Stop kills it. Every frame, the
// output that arrived is added to the terminal.
// After a run: each plot it made opens a Graph tab, or refreshes the one the
// same document's plot already has. The first new tab is shown.
inline void ShowRunResults(Editor& editor) {
    const auto plots = run::LatestPlots(runJob.TakeResults());
    if (plots.empty()) {
        return;
    }
    const Tab* source = FileIndex(editor.Files(), terminalFile) != SIZE_MAX ? terminalFile
                                                                          : nullptr;
    const Tab* first = nullptr;
    for (const run::PlotData& plot : plots) {
        GraphTab* existing = nullptr;
        for (const auto& tab : editor.Files()) {
            auto* graph = dynamic_cast<GraphTab*>(tab.get());
            if (graph && graph->Source() == source && graph->PlotId() == plot.id) {
                existing = graph;
            }
        }
        if (existing) {
            existing->SetPlot(plot);
            continue;
        }
        const Tab* active = editor.ActiveTab();
        const Tab* opened = editor.OpenTab(std::make_unique<GraphTab>(plot, source));
        editor.ActivateTab(active);
        if (!first) {
            first = opened;
        }
    }
    if (first) {
        SwitchTab(editor, FileIndex(editor.Files(), first));
    }
}

inline void ProcessRunRequest(Editor& editor) {
    if (stopRequested) {
        stopRequested = false;
        runJob.Stop();
    }
    if (runRequested) {
        runRequested = false;
        // From a Graph tab, Run runs the document that made it.
        if (const auto* graph = dynamic_cast<const GraphTab*>(editor.ActiveTab());
            graph && FileIndex(editor.Files(), graph->Source()) != SIZE_MAX) {
            SwitchTab(editor, FileIndex(editor.Files(), graph->Source()));
        }
        if (const FileBase* file = editor.ActiveFile()) {
            terminalOutput.clear();
            terminalOutput.push_back({run::OutputLine::Kind::Info, "Running " + file->TabName()});
            Translation translation = TranslateFile(*file);
            terminalFile = file;
            if (auto* document = dynamic_cast<FileMpp*>(editor.ActiveTab())) {
                document->checkWhileEditing = true;
                document->diagnostics = translation.diagnostics;
                terminalFileVersion = document->ContentVersion();
                checkedVersions[document] = terminalFileVersion;
            }
            if (translation.succeeded) {
                runLocations = std::move(translation.locations);
                runJob.Start(std::move(translation.cpp));
            } else {
                for (auto& error : translation.errors) {
                    terminalOutput.push_back({run::OutputLine::Kind::Error, std::move(error)});
                }
                for (const auto& problem : translation.diagnostics) {
                    terminalOutput.push_back(ProblemLine(problem));
                }
                const std::size_t count = translation.diagnostics.size();
                if (count > 0) {
                    terminalOutput.push_back(
                        {run::OutputLine::Kind::Info,
                         count == 1 ? "Fix the problem above to run the document (click it to "
                                      "go there)."
                                    : "Fix the " + std::to_string(count) +
                                          " problems above to run the document (click one to "
                                          "go there)."});
                }
            }
            terminalScrollToEnd = 2;
        }
    }
    auto output = runJob.TakeOutput();
    if (!output.empty()) {
        for (auto& line : output) {
            if (line.kind == run::OutputLine::Kind::Error) {
                AddRuntimeErrorSource(editor, line);
            }
        }
        terminalOutput.insert(terminalOutput.end(), std::make_move_iterator(output.begin()),
                              std::make_move_iterator(output.end()));
        terminalScrollToEnd = 2;
    }
    if (!runJob.Running()) {
        ShowRunResults(editor);
    }
    if (terminalJumpRequested != SIZE_MAX) {
        ProcessTerminalJump(editor);
    }
    CheckWhileEditing(editor);
}

inline bool SaveActiveFile(Editor& editor,
                           std::string& error) {
    FileBase* file = editor.ActiveFile();
    if (!file) {
        return false;
    }
    if (file->path.empty() || editor.IsTemporaryFile(*file)) {
        auto path = ShowSaveFileDialog(file->path.empty() ? std::filesystem::path("untitled.mpp")
                                                          : file->path.filename(),
                                       error);
        return path && file->Save(*path, error);
    }
    return file->Save(file->path, error);
}

// Apply the button the user picked in the unsaved-changes popup (if any).
inline void ProcessUnsavedChangesAction(Editor& editor) {
    const CloseAction action = closeAction;
    closeAction = CloseAction::NONE;
    if (action == CloseAction::CANCEL) {
        unsavedChangesOpen = false;
        closeError.clear();
        return;
    }
    // A cancelled or failed save keeps the document and popup open.
    if (action == CloseAction::DISCARD ||
        (action == CloseAction::SAVE && SaveActiveFile(editor, closeError))) {
        CloseActiveTab(editor);
        unsavedChangesOpen = false;
        closeError.clear();
    }
}

inline void ProcessTabRequests(Editor& editor) {
    if (closeTabRequested != SIZE_MAX) {
        // Closing a background tab brings it to the front first, so the
        // unsaved-changes popup and the close logic act on the right file.
        SwitchTab(editor, closeTabRequested);
        closeTabRequested = SIZE_MAX;
        selectTabRequested = SIZE_MAX;
        closeRequested = true;
    }
    if (selectTabRequested != SIZE_MAX) {
        SwitchTab(editor, selectTabRequested);
        selectTabRequested = SIZE_MAX;
    }
}

// Close the active tab, or ask first if it has unsaved changes.
inline void ProcessCloseRequest(Editor& editor) {
    closeRequested = false;
    if (Tab* file = editor.ActiveTab()) {
        if (file->modified) {
            unsavedChangesOpen = true;
            CloseAllMenus();
            closeError.clear();
        } else {
            CloseActiveTab(editor);
        }
    }
}

inline void ProcessNewRequest(Editor& editor,
                              std::string& error) {
    if (!newRequested) {
        return;
    }
    newRequested = false;
    if (!editor.NewTemporaryFile(error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
    }
}

inline void ProcessSaveRequest(Editor& editor,
                               std::string& error) {
    if (!saveRequested) {
        return;
    }
    saveRequested = false;
    SaveActiveFile(editor, error);
    if (!error.empty()) {
        std::fprintf(stderr, "%s\n", error.c_str());
    }
}

inline void ProcessDocsRequest(Editor& editor,
                               std::string& error) {
    if (!docsRequested) {
        return;
    }
    docsRequested = false;
    std::filesystem::path path = "assets/doc/help.mpp";
    std::error_code filesystemError;
    if (!std::filesystem::exists(path, filesystemError)) {
        path = std::filesystem::path(GetApplicationDirectory()) / path;
    }
    if (!editor.OpenFile(path, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
    }
}

inline void ProcessOpenRequest(Editor& editor,
                               std::string& error) {
    if (!openRequested) {
        return;
    }
    openRequested = false;
    auto path = ShowOpenFileDialog(error);
    if (path) {
        editor.OpenFile(*path, error);
    }

    if (!error.empty()) {
        std::fprintf(stderr, "%s\n", error.c_str());
    }
}
}  // namespace layout_internal
