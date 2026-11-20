#pragma once

// Tokens: what the parser reads. src/lang/Lower turns a document's node tree
// into tokens; every structure (fraction, root, power, ...) arrives as a
// single token holding the tokens of each of its slots.

#include "Source.h"

#include <string>
#include <vector>

namespace lang {

struct Token {
    enum class Kind {
        Number,     // text: "12", "3.5", ".5", "1e-3"
        Word,       // a name or keyword: "x", "mod", "α"
        String,     // text: the contents, without quotes
        Op,         // an operator or punctuation, by its canonical glyph (see below)
        Structure,  // a fraction, root, power, matrix, ... with its slots
        Newline,    // end of a document row
        End,        // end of the tokens
    };

    // What kind of structure a Structure token is.
    enum class Shape {
        None,
        Fraction,  // slots: {numerator, denominator}
        Root,      // slots: {argument} or {argument, index}
        Power,     // slots: {base, exponent}; an empty base raises what comes before
        Group,     // slots: {contents}, a bare {...} group from the source
        Matrix,    // slots: the cells, row by row (rowLengths: cells per row);
                   // text: the environment ("pmatrix", "vmatrix", ...)
        Cases,     // slots: value, condition, value, condition, ... (rowLengths)
        Delimited, // text "abs", "floor", "ceil" or "norm"; slots: {contents}
        Subscript, // slots: {index}; belongs to what comes before it
        BigOp,     // text "sum", "prod", "bigcup" or "bigcap"; slots: {lower, upper}
        Graph,
        Table,
    };

    Kind kind = Kind::End;

    // Number text, Word as written ("α" for a Greek letter), String contents,
    // or the Op. Ops are canonical, whichever way they were typed:
    //   + − ⋅ × ⊙ / ^ ( ) [ ] { } , ; : := ← → = ≠ < ≤ > ≥ ≈ ∧ ∨ ¬ ⊕ ∈ ∉ ...
    //   and the built-in sets ℕ ℤ ℚ ℝ ℂ 𝔹 Σ and ∞ ∅ ∀ ∃ ⊤.
    std::string text;

    // Word: a name safe to use in C++ ("x", or "alpha" for α).
    std::string ident;
    bool greek = false;

    SourceRef ref;

    Shape shape = Shape::None;
    // Structures: the tokens of each slot. A template string ("Hi ${name}")
    // has the tokens of each ${…} here, and its text parts in `pieces`.
    std::vector<std::vector<Token>> slots;
    std::vector<std::string> pieces;
    std::vector<std::size_t> rowLengths;

    bool Is(Kind k, std::string_view t) const { return kind == k && text == t; }
    bool IsOp(std::string_view t) const { return Is(Kind::Op, t); }
    bool IsWord(std::string_view t) const { return Is(Kind::Word, t); }
};

}  // namespace lang
