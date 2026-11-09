#pragma once

#include "EditorNode.h"

#include <optional>
#include <string>
#include <string_view>

// A lowercase Greek letter such as \alpha.
class GreekNode : public EditorNode {
public:
    enum GreekSymbolType {
        alpha,
        beta,
        gamma,
        delta,
        epsilon,
        zeta,
        eta,
        theta,
        iota,
        kappa,
        lambda,
        mu,
        nu,
        xi,
        omicron,
        pi,
        rho,
        sigma,
        tau,
        upsilon,
        phi,
        chi,
        psi,
        omega
    };

    explicit GreekNode(GreekSymbolType symbol);

    ~GreekNode() override = default;

    void Render(float fontSize) override;

    void KeyPress() override;

    void MouseClick() override;

    std::string ToText() const override;

    std::pair<float, float> GetNWCorner() override;

    std::pair<float, float> GetSize() override;

    bool HasChild() override;

    GreekSymbolType GetSymbol() const;

    // Name of the symbol without the backslash, e.g. "alpha".
    static std::string_view SymbolName(GreekSymbolType symbol);

    // Look up a symbol by name without the backslash, e.g. "alpha".
    static std::optional<GreekSymbolType> SymbolFromName(std::string_view name);

private:
    GreekSymbolType symbol_;
};
