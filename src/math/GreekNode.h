#pragma once

#include "EditorNode.h"

#include <optional>
#include <string>
#include <string_view>

// A lowercase Greek letter such as \alpha.
class GreekNode : public EditorNode {
public:
    // Keep values in table order; the GREEK_ prefix avoids graphics-library macros.
    enum GreekSymbolType {
        GREEK_ALPHA,
        GREEK_BETA,
        GREEK_GAMMA,
        GREEK_DELTA,
        GREEK_EPSILON,
        GREEK_ZETA,
        GREEK_ETA,
        GREEK_THETA,
        GREEK_IOTA,
        GREEK_KAPPA,
        GREEK_LAMBDA,
        GREEK_MU,
        GREEK_NU,
        GREEK_XI,
        GREEK_OMICRON,
        GREEK_PI,
        GREEK_RHO,
        GREEK_SIGMA,
        GREEK_TAU,
        GREEK_UPSILON,
        GREEK_PHI,
        GREEK_CHI,
        GREEK_PSI,
        GREEK_OMEGA
    };

    explicit GreekNode(GreekSymbolType symbol);

    ~GreekNode() override = default;

    void Render(const RenderContext& context) override;
    std::unique_ptr<EditorNode> Clone() const override;

    void MouseClick() override;

    std::string ToText() const override;

    std::pair<float,
              float>
    GetNWCorner() override;

    std::pair<float,
              float>
    GetSize() override;

    bool HasChild() override;

    GreekSymbolType GetSymbol() const;

    // Name of the symbol without the backslash, e.g. "alpha".
    static std::string_view SymbolName(GreekSymbolType symbol);

    // Look up a symbol by name without the backslash, e.g. "alpha".
    static std::optional<GreekSymbolType> SymbolFromName(std::string_view name);

private:
    GreekSymbolType symbol_;
};
