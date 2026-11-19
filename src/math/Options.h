#pragma once

// Reading and writing the key=value options that objects store in their
// optional argument, e.g. the [ ... ] in \begin{graph}[title=Growth, xmin=0].
// Shared by graphs and tables.
//
// Format: items separated by commas; each is key=value, or just a key. A value
// that would otherwise be misread (it contains , = [ ] \, starts with "{", or
// has a space at either end) is wrapped in braces: title={a, b}. Braces inside
// values must be balanced, since the whole argument is LaTeX.

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace options {

using Option = std::pair<std::string, std::string>;  // key, value (unwrapped)

// Split options into key/value pairs, in order. Keys are trimmed; values are
// trimmed and unwrapped. A bare key has an empty value.
std::vector<Option> Parse(std::string_view text);

// A value ready to write (wrapped in braces when needed).
std::string Wrap(const std::string& value);

// "key=value" with the value wrapped as needed, or just "key" for an empty value.
std::string Item(const std::string& key,
                 const std::string& value);

// Items joined with ", ".
std::string Join(const std::vector<std::string>& items);

// Shortest text that reads back as the same number: "-10", "2.5", "1e-05".
std::string FormatNumber(double value);
// Parse a whole string; false (value unchanged) if any of it isn't part of one.
bool ParseNumber(std::string_view text,
                 double& value);
// "true" or "false"; false (value unchanged) for anything else.
bool ParseBool(std::string_view text,
               bool& value);

// An object's source: \begin{environment}[options]body\end{environment}.
// The brackets are left out when there are no options, unless the body
// starts with "[" (it would then be read back as the options).
std::string Environment(const std::string& environment,
                        const std::string& options,
                        const std::string& body);

}  // namespace options
