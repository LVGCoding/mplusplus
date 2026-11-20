#pragma once

// Shared helpers for M++ unit tests.

#include "math/EditorNode.h"
#include "math/ExpoNode.h"
#include "math/FracNode.h"
#include "math/GreekNode.h"
#include "math/LineNode.h"
#include "math/MatrixNode.h"
#include "math/SqrtNode.h"
#include "math/SumNode.h"
#include "math/TextNode.h"
#include "parser/parser.h"

#include <memory>
#include <string>
#include <vector>

namespace test {

std::string Describe(const EditorNode* node);

template <typename Node>
std::string DescribeList(const std::vector<std::unique_ptr<Node>>& nodes) {
    std::string description;
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        if (i > 0) {
            description += ", ";
        }
        description += Describe(nodes[i].get());
    }
    return description;
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
        std::string description = line->IsBraced() ? "group(" : "line(";
        for (std::size_t i = 0; i < line->ChildCount(); ++i) {
            if (i > 0) {
                description += ", ";
            }
            description += Describe(line->GetChild(i));
        }
        return description + ")";
    }
    if (auto* fraction = dynamic_cast<const FracNode*>(node)) {
        auto [numerator, denominator] = fraction->GetChildren();
        return "frac(" + Describe(numerator) + ", " + Describe(denominator) + ")";
    }
    if (auto* exponent = dynamic_cast<const ExpoNode*>(node)) {
        return "exp(" + Describe(exponent->GetBase()) + ", " +
               Describe(exponent->GetExponent()) + ")";
    }
    if (auto* squareRoot = dynamic_cast<const SqrtNode*>(node)) {
        auto [root, argument] = squareRoot->GetChildren();
        std::string description = "sqrt";
        if (root) {
            description += "[" + Describe(root) + "]";
        }
        return description + "(" + Describe(argument) + ")";
    }
    if (auto* sum = dynamic_cast<const SumNode*>(node)) {
        auto [summand, index, upperLimit] = sum->GetChildren();
        return "sum(" + Describe(index) + ", " + Describe(upperLimit) +
               ", " + Describe(summand) + ")";
    }
    if (auto* matrix = dynamic_cast<const MatrixNode*>(node)) {
        std::string description = matrix->GetEnvironment() + "[";
        for (std::size_t row = 0; row < matrix->RowCount(); ++row) {
            description += row > 0 ? ", [" : "[";
            for (std::size_t column = 0; column < matrix->ColumnCount(row); ++column) {
                if (column > 0) {
                    description += ", ";
                }
                description += Describe(matrix->GetCell(row, column));
            }
            description += "]";
        }
        return description + "]";
    }
    return "unknown";
}

// Parse `source` and describe the resulting top-level nodes.
inline std::string ParseAndDescribe(const std::string& source) {
    return DescribeList(Parser().ParseText(source));
}

// Parse `source` and serialize it back with ToText().
inline std::string RoundTrip(const std::string& source) {
    std::string serializedText;
    for (const auto& node : Parser().ParseText(source)) {
        serializedText += node->ToText();
    }
    return serializedText;
}

}  // namespace test
