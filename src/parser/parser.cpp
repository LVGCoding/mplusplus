#include "math/EditorNode.h"
#include <string>
#include <vector>
#include "parser.h"
 
std::vector<EditorNode> Parser::ParseText(std::string inputText) {
    std::vector tokens = Tokenizer(inputText);
}

std::vector<EditorNode> Parser::Tokenizer(std::string inputText) {
    std::string currentToken = "";
    int index = 0;
    while (index < inputText.length()) {
        if (inputText[index] == ' ' && inputText[index] == '\\' && inputText[index] == '{' &&
            inputText[index] == '}' && inputText[index] == '[' && inputText[index] == ']') {
        }
    }
}
