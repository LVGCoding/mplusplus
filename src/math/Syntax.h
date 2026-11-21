#pragma once

// Syntax highlighting: which parts of a document's text get which color.
// src/lang/Highlight fills a SyntaxColors from the tokens of each row; the
// nodes read it while rendering (RenderContext::syntax).

#include "clay.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

class EditorNode;

enum class SyntaxKind : std::uint8_t {
    Plain,     // names, operators, punctuation
    Keyword,   // if, else, for, return, mod, in, true, ...
    Number,    // 12, 3.5
    String,    // "text", quotes included
    Function,  // a name called with (…): print(x), f(2)
    Type,      // the built-in sets ℕ ℤ ℚ ℝ ℂ 𝔹 Σ
    Constant,  // ∞ ∅
};

inline Clay_Color SyntaxColor(SyntaxKind kind) {
    switch (kind) {
    case SyntaxKind::Keyword: return {160, 40, 160, 255};
    case SyntaxKind::Number: return {25, 115, 140, 255};
    case SyntaxKind::String: return {40, 130, 50, 255};
    case SyntaxKind::Function: return {30, 80, 190, 255};
    case SyntaxKind::Type: return {175, 95, 0, 255};
    case SyntaxKind::Constant: return {25, 115, 140, 255};
    case SyntaxKind::Plain: break;
    }
    return {20, 20, 20, 255};
}

struct SyntaxColors {
    // A TextNode's kind per byte of its text (same length as the text).
    std::unordered_map<const EditorNode*, std::vector<SyntaxKind>> text;
    // A Greek letter or symbol drawn in one color.
    std::unordered_map<const EditorNode*, SyntaxKind> nodes;

    void Clear() {
        text.clear();
        nodes.clear();
    }
};
