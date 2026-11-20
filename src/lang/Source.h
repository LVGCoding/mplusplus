#pragma once

// Where things are in a document, so errors can point at them.

#include "math/EditorNode.h"

#include <string>

namespace lang {

// A range of a document, as two cursor paths (see CursorPath): from `start`
// to `end`, both full paths [row, ..., text child, byte offset]. Like a
// selection, both ends are in the same LineNode, or directly in two rows for
// a range spanning rows. An empty `start` means "nowhere in particular".
struct SourceRef {
    CursorPath start;
    CursorPath end;

    bool Empty() const { return start.empty(); }
    // The document row (0-based) the range starts in.
    std::size_t Row() const { return start.empty() ? 0 : start[0]; }

    bool operator==(const SourceRef&) const = default;
};

// From the start of `first` to the end of `last`. If they aren't in the same
// line (or both directly in rows), this is just `first`.
SourceRef Span(const SourceRef& first, const SourceRef& last);

// A problem found before running: a message and where it is.
struct Diagnostic {
    std::string message;
    SourceRef ref;
};

}  // namespace lang
