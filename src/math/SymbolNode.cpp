#include "SymbolNode.h"

#include "NodeStyle.h"
#include "ui/font.h"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>

namespace {
// The font (FONT_MATH) must load these glyphs; LoadFontTable reads this table.
constexpr auto R = SymbolNode::Kind::Relation;
constexpr auto O = SymbolNode::Kind::Operator;

constexpr std::array<SymbolNode::Symbol, 34> SYMBOLS = {{
    // Built-in sets.
    {"mathbb{N}", "ℕ"},
    {"mathbb{Z}", "ℤ"},
    {"mathbb{Q}", "ℚ"},
    {"mathbb{R}", "ℝ"},
    {"mathbb{C}", "ℂ"},
    {"mathbb{B}", "𝔹"},
    {"Sigma", "Σ"},
    // Sets.
    {"in", "∈", R},
    {"notin", "∉", R},
    {"subset", "⊂", R},
    {"subseteq", "⊆", R},
    {"cup", "∪", O},
    {"cap", "∩", O},
    {"setminus", "∖", O},
    {"Delta", "Δ"},  // also a name prefix (Δx), so no operator spacing
    // Assignment, functions.
    {"leftarrow", "←", R},
    {"to", "→", R},
    {"mapsto", "↦", R},
    // Arithmetic.
    {"cdot", "⋅", O},
    {"times", "×", O},
    {"odot", "⊙", O},
    {"top", "⊤"},
    {"infty", "∞"},
    {"emptyset", "∅"},
    // Comparison.
    {"neq", "≠", R},
    {"leq", "≤", R},
    {"geq", "≥", R},
    {"approx", "≈", R},
    // Logic.
    {"land", "∧", O},
    {"lor", "∨", O},
    {"neg", "¬"},
    {"oplus", "⊕", O},
    {"forall", "∀"},
    {"exists", "∃"},
}};
}  // namespace

std::span<const SymbolNode::Symbol> SymbolNode::Symbols() {
    return SYMBOLS;
}

std::optional<std::size_t> SymbolNode::IndexOf(std::string_view name) {
    for (std::size_t i = 0; i < SYMBOLS.size(); ++i) {
        if (SYMBOLS[i].name == name) {
            return i;
        }
    }
    return std::nullopt;
}

SymbolNode::SymbolNode(std::size_t index) : index_(index) {
    assert(index < SYMBOLS.size());
}

SymbolNode::SymbolNode(std::string_view name) : SymbolNode(IndexOf(name).value_or(0)) {
    assert(IndexOf(name));
}

const SymbolNode::Symbol& SymbolNode::GetSymbol() const {
    return SYMBOLS[index_];
}

void SymbolNode::Render(const RenderContext& context) {
    const float room = GetSymbol().kind == Kind::Relation   ? 0.25f
                       : GetSymbol().kind == Kind::Operator ? 0.18f
                                                            : 0.0f;
    const auto side = static_cast<uint16_t>(std::lround(room * context.fontSize));
    CLAY(GetClayId(), {.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                                  .padding = {side, side, 0, 0}}}) {
        node_style::Text(GetSymbol().glyph, context.fontSize, FONT_MATH);
    }
}

std::unique_ptr<EditorNode> SymbolNode::Clone() const {
    return std::make_unique<SymbolNode>(*this);
}

std::string SymbolNode::ToText() const {
    return "\\" + std::string(GetSymbol().name);
}
