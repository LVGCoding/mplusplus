#pragma once

#include "EditorNode.h"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>

// A math symbol that is one unit in the document, such as ∈, ℝ or ←. Like a
// Greek letter, the cursor steps over it. Written in source as its LaTeX
// command (\in, \mathbb{R}, \leftarrow) and drawn with the math font.
//
// Greek letters stay GreekNodes: they are names (variables), while these are
// operators and built-in sets, which the language treats differently.
class SymbolNode : public EditorNode {
public:
    // How a symbol is spaced, as in TeX: relations (∈, ≤, ←) get the most
    // room on each side, binary operators (∪, ⋅) a little less, and ordinary
    // symbols (ℝ, ∞) and prefixes (∀, ¬) none.
    enum class Kind { Ordinary, Operator, Relation };

    struct Symbol {
        // The LaTeX command without the backslash: "in", "mathbb{R}".
        std::string_view name;
        // What is drawn, as UTF-8.
        std::string_view glyph;
        Kind kind = Kind::Ordinary;
    };

    // Every symbol, in the order the math menu lists them.
    static std::span<const Symbol> Symbols();

    // A symbol by its name (without the backslash), e.g. "in" or "mathbb{R}".
    static std::optional<std::size_t> IndexOf(std::string_view name);

    // `index` into Symbols().
    explicit SymbolNode(std::size_t index);
    // Must be a name from Symbols().
    explicit SymbolNode(std::string_view name);

    const Symbol& GetSymbol() const;
    std::string_view Name() const { return GetSymbol().name; }

    void Render(const RenderContext& context) override;
    std::unique_ptr<EditorNode> Clone() const override;
    void MouseClick() override {}
    std::string ToText() const override;
    std::pair<float, float> GetNWCorner() override { return {0.0f, 0.0f}; }
    std::pair<float, float> GetSize() override { return {0.0f, 0.0f}; }
    bool HasChild() override { return false; }

private:
    std::size_t index_;
};
