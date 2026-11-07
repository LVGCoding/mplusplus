#include "svg_preprocessor.h"

#include <unordered_map>
#include <iostream>
#include <string>
#include <array>
#include <cctype>
#include <fstream>
#include <sstream>

// Reads an attribute from an SVG element.
// Supports both single and double quotes.
static std::string GetAttribute(const std::string& element, const std::string& attribute) {
    size_t position = 0;

    while (true) {
        position = element.find(attribute, position);

        if (position == std::string::npos)
            return "";

        // Make sure this is the beginning of an attribute.
        if (position > 0 &&
            !std::isspace(static_cast<unsigned char>(element[position - 1]))) {
            position += attribute.length();
            continue;
        }

        position += attribute.length();

        while (position < element.size() &&
               std::isspace(static_cast<unsigned char>(element[position]))) {
            position++;
        }

        if (position >= element.size() || element[position] != '=') {
            continue;
        }

        position++;

        while (position < element.size() &&
               std::isspace(static_cast<unsigned char>(element[position]))) {
            position++;
        }

        if (position >= element.size())
            return "";

        char quote = element[position];

        if (quote != '\'' && quote != '"')
            return "";

        size_t end = element.find(quote, position + 1);

        if (end == std::string::npos)
            return "";

        return element.substr(position + 1, end - position - 1);
    }
}

// Escapes characters that could interfere with an XML attribute.
static std::string EscapeXML(const std::string& value) {
    std::string result;

    for (char c : value) {
        switch (c) {
            case '&':  result += "&amp;";  break;
            case '"':  result += "&quot;"; break;
            case '<':  result += "&lt;";   break;
            default:   result += c;        break;
        }
    }

    return result;
}

bool SVGPreprocessor::Process(const std::filesystem::path& inputPath, const std::filesystem::path& outputPath) {
    std::ifstream file(inputPath);

    if (!file.is_open()) {
        std::cerr << "Failed to open input SVG: " << inputPath << "\n";
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    std::string svg = buffer.str();

    // Keep the existing extraction and replacement logic here.

    // Maps SVG path IDs to their actual path data.
    std::unordered_map<std::string, std::string> paths;

    // --------------------------------------------------
    // STEP 1: Extract all path definitions from <defs>
    // --------------------------------------------------

    size_t position = svg.find("<path");

    while (position != std::string::npos) {

        size_t end = svg.find('>', position);

        if (end == std::string::npos)
            break;

        std::string element = svg.substr(
            position,
            end - position + 1
        );

        std::string id = GetAttribute(element, "id");
        std::string d = GetAttribute(element, "d");

        if (!id.empty() && !d.empty()) {
            paths[id] = d;

            std::cout << "Found path: " << id << '\n';
        }

        position = svg.find("<path", end + 1);
    }

    // --------------------------------------------------
    // STEP 2: Replace <use> elements with actual paths
    // --------------------------------------------------

    std::string result;
    size_t currentPosition = 0;

    while (true) {

        size_t useStart = svg.find("<use", currentPosition);

        // No more <use> elements.
        if (useStart == std::string::npos) {
            result += svg.substr(currentPosition);
            break;
        }

        size_t useEnd = svg.find('>', useStart);

        if (useEnd == std::string::npos) {
            result += svg.substr(currentPosition);
            break;
        }

        // Copy everything before this <use>.
        result += svg.substr(
            currentPosition,
            useStart - currentPosition
        );

        std::string useElement = svg.substr(
            useStart,
            useEnd - useStart + 1
        );

        // Get the referenced path ID.
        std::string href = GetAttribute(useElement, "xlink:href");

        if (href.empty())
            href = GetAttribute(useElement, "href");

        // Remove the # from references such as #g0-11.
        if (!href.empty() && href[0] == '#')
            href.erase(0, 1);

        auto pathIt = paths.find(href);

        // If the reference cannot be found, leave the original element.
        if (pathIt == paths.end()) {

            std::cerr << "Could not find referenced SVG path: "
                      << href << '\n';

            result += useElement;
        }
        else {

            std::string x = GetAttribute(useElement, "x");
            std::string y = GetAttribute(useElement, "y");

            if (x.empty()) x = "0";
            if (y.empty()) y = "0";

            // Create a real path instead of a <use> reference.
            result += "<path d=\"" + EscapeXML(pathIt->second) + "\"";

            result += " transform=\"translate(" + x + "," + y + ")\"";

            // Preserve presentation attributes from the original <use>.
            const std::array<const char*, 7> attributes = {
                "style",
                "fill",
                "stroke",
                "fill-rule",
                "clip-rule",
                "opacity",
                "class"
            };

            for (const char* attribute : attributes) {
                std::string value = GetAttribute(useElement, attribute);

                if (!value.empty()) {
                    result += " ";
                    result += attribute;
                    result += "=\"" + EscapeXML(value) + "\"";
                }
            }

            result += "/>";
        }

        currentPosition = useEnd + 1;
    }

    std::ofstream output(outputPath);

    if (!output.is_open()) {
        std::cerr << "Failed to create processed SVG: " << outputPath << '\n';
        return false;
    }

    output << result;

    return output.good();
}