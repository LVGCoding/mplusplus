#include "GreekNode.h"

#include "NodeStyle.h"

#include <array>

namespace {
// Indexed by GreekNode::GreekSymbolType; keep in the same order as the enum.
constexpr std::array<std::string_view, 24> SYMBOL_NAMES = {
    "alpha", "beta",  "gamma",  "delta",   "epsilon", "zeta", "eta",     "theta",
    "iota",  "kappa", "lambda", "mu",      "nu",      "xi",   "omicron", "pi",
    "rho",   "sigma", "tau",    "upsilon", "phi",     "chi",  "psi",     "omega",
};

// UTF-8 glyph for each symbol, in the same order as SYMBOL_NAMES. The font must
// load these codepoints (see LoadFontTable).
constexpr std::array<std::string_view, 24> SYMBOL_GLYPHS = {
    "α", "β", "γ", "δ", "ε", "ζ", "η", "θ", "ι", "κ", "λ", "μ",
    "ν", "ξ", "ο", "π", "ρ", "σ", "τ", "υ", "φ", "χ", "ψ", "ω",
};
static_assert(SYMBOL_NAMES.size() == static_cast<std::size_t>(GreekNode::GREEK_OMEGA) + 1);
static_assert(SYMBOL_GLYPHS.size() == SYMBOL_NAMES.size());
}  // namespace

GreekNode::GreekNode(GreekSymbolType symbol) : symbol_(symbol) {}

// A single symbol: the cursor steps over it rather than going inside.
void GreekNode::Render(const RenderContext& context) {
    CLAY(GetClayId(), {.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)}}}) {
        node_style::Text(SYMBOL_GLYPHS[static_cast<std::size_t>(symbol_)], context.fontSize);
    }
}
void GreekNode::MouseClick() {}
std::pair<float,
          float>
GreekNode::GetNWCorner() {
    return {0.0f, 0.0f};
}
std::pair<float,
          float>
GreekNode::GetSize() {
    return {0.0f, 0.0f};
}

std::string GreekNode::ToText() const {
    return "\\" + std::string(SymbolName(symbol_));
}

bool GreekNode::HasChild() {
    return false;
}

GreekNode::GreekSymbolType GreekNode::GetSymbol() const {
    return symbol_;
}

std::string_view GreekNode::SymbolName(GreekSymbolType symbol) {
    return SYMBOL_NAMES[static_cast<std::size_t>(symbol)];
}

std::optional<GreekNode::GreekSymbolType> GreekNode::SymbolFromName(std::string_view name) {
    for (std::size_t i = 0; i < SYMBOL_NAMES.size(); ++i) {
        if (SYMBOL_NAMES[i] == name) {
            return static_cast<GreekSymbolType>(i);
        }
    }
    return std::nullopt;
}

std::unique_ptr<EditorNode> GreekNode::Clone() const {
    return std::make_unique<GreekNode>(*this);
}
