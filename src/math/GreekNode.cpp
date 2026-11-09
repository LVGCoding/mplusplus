#include "GreekNode.h"

#include <array>

namespace {
// Indexed by GreekNode::GreekSymbolType; keep in the same order as the enum.
constexpr std::array<std::string_view, 24> symbolNames = {
    "alpha", "beta",    "gamma", "delta", "epsilon", "zeta",    "eta", "theta",
    "iota",  "kappa",   "lambda", "mu",   "nu",      "xi",      "omicron", "pi",
    "rho",   "sigma",   "tau",   "upsilon", "phi",   "chi",     "psi", "omega",
};
}  // namespace

GreekNode::GreekNode(GreekSymbolType symbol) : symbol_(symbol) {}

// TODO: rendering and input are not implemented yet.
void GreekNode::Render(float /*fontSize*/) {}
void GreekNode::KeyPress() {}
void GreekNode::MouseClick() {}
std::pair<float, float> GreekNode::GetNWCorner() {
    return {0.0f, 0.0f};
}
std::pair<float, float> GreekNode::GetSize() {
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
    return symbolNames[static_cast<std::size_t>(symbol)];
}

std::optional<GreekNode::GreekSymbolType> GreekNode::SymbolFromName(std::string_view name) {
    for (std::size_t i = 0; i < symbolNames.size(); ++i) {
        if (symbolNames[i] == name) {
            return static_cast<GreekSymbolType>(i);
        }
    }
    return std::nullopt;
}
