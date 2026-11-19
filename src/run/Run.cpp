#include "Run.h"

#include "editor/FileMpp.h"
#include "editor/fileBase.h"

RunResult RunFile(const FileBase& file) {
    RunResult result;
    result.output.push_back("Running " + file.TabName() + "...");

    const auto* document = dynamic_cast<const FileMpp*>(&file);
    if (!document) {
        result.succeeded = false;
        result.output.push_back("Only M++ (.mpp) documents can be run.");
        return result;
    }

    // TODO: implement running. `document->content` holds one LineNode per row
    // (see math/EditorNode.h); walk it and append results to result.output.
    result.output.push_back("Run is not implemented yet.");
    return result;
}
