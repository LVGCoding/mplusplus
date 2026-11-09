#pragma once

// Shared helpers for M++ unit tests.

#include "math/EditorNode.h"
#include "math/FracNode.h"
#include "math/GreekNode.h"
#include "math/LineNode.h"
#include "math/MatrixNode.h"
#include "math/SqrtNode.h"
#include "math/TextNode.h"
#include "parser/parser.h"

#include <memory>
#include <string>
#include <vector>

namespace test {

std::string Describe(const EditorNode* node);

inline std::string DescribeList(const std::vector<std::unique_ptr<EditorNode>>& nodes) {
    std::string out;
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        if (i > 0) {
            out += ", ";
        }
        out += Describe(nodes[i].get());
    }
    return out;
}

// Compact, readable description of a node tree, so tests can compare whole
// structures in a single EXPECT_EQ. Examples:
//   text("x+")   greek(alpha)   frac(line(text("a")), line(text("b")))
//   sqrt[line(text("3"))](line(text("x")))   group(text("2"))
//   pmatrix[[line(text("a")), line(text("b"))], [line(text("c")), line(text("d"))]]
inline std::string Describe(const EditorNode* node) {
    if (!node) {
        return "null";
    }
    if (auto* text = dynamic_cast<const TextNode*>(node)) {
        return "text(\"" + text->GetText() + "\")";
    }
    if (auto* greek = dynamic_cast<const GreekNode*>(node)) {
        return "greek(" + std::string(GreekNode::SymbolName(greek->GetSymbol())) + ")";
    }
    if (auto* line = dynamic_cast<const LineNode*>(node)) {
        std::string out = line->IsBraced() ? "group(" : "line(";
        for (std::size_t i = 0; i < line->ChildCount(); ++i) {
            if (i > 0) {
                out += ", ";
            }
            out += Describe(line->GetChild(i));
        }
        return out + ")";
    }
    if (auto* frac = dynamic_cast<const FracNode*>(node)) {
        auto [numerator, denominator] = frac->GetChildren();
        return "frac(" + Describe(numerator) + ", " + Describe(denominator) + ")";
    }
    if (auto* sqrt = dynamic_cast<const SqrtNode*>(node)) {
        auto [root, argument] = sqrt->GetChildren();
        std::string out = "sqrt";
        if (root) {
            out += "[" + Describe(root) + "]";
        }
        return out + "(" + Describe(argument) + ")";
    }
    if (auto* matrix = dynamic_cast<const MatrixNode*>(node)) {
        std::string out = matrix->GetEnvironment() + "[";
        for (std::size_t row = 0; row < matrix->RowCount(); ++row) {
            out += row > 0 ? ", [" : "[";
            for (std::size_t column = 0; column < matrix->ColumnCount(row); ++column) {
                if (column > 0) {
                    out += ", ";
                }
                out += Describe(matrix->GetCell(row, column));
            }
            out += "]";
        }
        return out + "]";
    }
    return "unknown";
}

// Parse `source` and describe the resulting top-level nodes.
inline std::string ParseAndDescribe(const std::string& source) {
    return DescribeList(Parser().ParseText(source));
}

// Parse `source` and serialize it back with ToText().
inline std::string RoundTrip(const std::string& source) {
    std::string out;
    for (const auto& node : Parser().ParseText(source)) {
        out += node->ToText();
    }
    return out;
}

}  // namespace test
