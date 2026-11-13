#pragma once

// Editor behaviour that will become user settings. For now these are fixed
// defaults; FileMpp copies them into members so tests can try each option.

namespace editor_settings {

// What Backspace does when the cursor is just after a fraction, root or matrix
// (or at the start of its first slot).
enum class StructureBackspace {
    // First press marks the object in red, a second press deletes it.
    HighlightThenDelete,
    // One press deletes it.
    DeleteImmediately,
};

inline constexpr StructureBackspace structureBackspace = StructureBackspace::HighlightThenDelete;

}  // namespace editor_settings
