#include "parser/parser.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using Tokens = std::vector<std::string>;

TEST(Tokenizer, EmptyInputHasNoTokens) {
    EXPECT_TRUE(Parser().Tokenizer("").empty());
}

TEST(Tokenizer, PlainTextIsOneToken) {
    EXPECT_EQ(Parser().Tokenizer("x+1"), (Tokens{"x+1"}));
}

TEST(Tokenizer, WhitespaceRunsAreSeparateTokens) {
    EXPECT_EQ(Parser().Tokenizer("a  b\nc"), (Tokens{"a", "  ", "b", "\n", "c"}));
}

TEST(Tokenizer, CommandIncludesItsLetters) {
    EXPECT_EQ(Parser().Tokenizer("\\frac{a}{b}"),
              (Tokens{"\\frac", "{", "a", "}", "{", "b", "}"}));
}

TEST(Tokenizer, CommandStopsAtNonLetter) {
    EXPECT_EQ(Parser().Tokenizer("\\alpha2"), (Tokens{"\\alpha", "2"}));
}

TEST(Tokenizer, EscapedSymbolIsOneToken) {
    EXPECT_EQ(Parser().Tokenizer("\\\\\\{\\}"), (Tokens{"\\\\", "\\{", "\\}"}));
}

TEST(Tokenizer, TrailingBackslashIsKept) {
    EXPECT_EQ(Parser().Tokenizer("a\\"), (Tokens{"a", "\\"}));
}

TEST(Tokenizer, DelimitersAreSingleTokens) {
    EXPECT_EQ(Parser().Tokenizer("[a]&{b}"), (Tokens{"[", "a", "]", "&", "{", "b", "}"}));
}

TEST(Tokenizer, TokensConcatenateToInput) {
    const std::string input = "x = \\frac{a}{b}  + \\sqrt[3]{y}\\\\ \\{z\\} & \\";
    std::string joined;
    for (const auto& token : Parser().Tokenizer(input)) {
        joined += token;
    }
    EXPECT_EQ(joined, input);
}
