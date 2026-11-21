#pragma once

// Double and triple clicks: counting quick clicks in one place, and finding
// the word to select. Shared by FileMpp and FileText; no raylib calls, so it
// can be tested (the caller passes the time and the mouse position).

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <string_view>
#include <utility>

class ClickCounter {
public:
    // Clicks closer together than this (seconds and pixels) count as one
    // double or triple click.
    static constexpr double MAX_GAP = 0.4;
    static constexpr float MAX_DISTANCE = 5.0f;

    // Record a press; returns 1 for a single click, 2 for a double, 3 for a
    // triple. A fourth quick click starts again at 1.
    int Press(double now, float x, float y) {
        const bool quick = count_ > 0 && count_ < 3 && now - time_ <= MAX_GAP &&
                           std::fabs(x - x_) <= MAX_DISTANCE &&
                           std::fabs(y - y_) <= MAX_DISTANCE;
        count_ = quick ? count_ + 1 : 1;
        time_ = now;
        x_ = x;
        y_ = y;
        return count_;
    }

private:
    int count_ = 0;
    double time_ = 0;
    float x_ = 0;
    float y_ = 0;
};

// The run of `text` a double click at byte `offset` selects, as [begin, end):
// a word (letters, digits, _ and any non-ASCII character), a run of spaces,
// or a run of other characters (operators, punctuation). Between a word and
// something else, the word wins.
inline std::pair<std::size_t, std::size_t> WordAt(std::string_view text, std::size_t offset) {
    auto kind = [](char c) {
        const auto byte = static_cast<unsigned char>(c);
        if (c == '\n') return 3;  // never joins a run
        if (c == ' ' || c == '\t') return 0;
        if (std::isalnum(byte) || c == '_' || byte >= 0x80) return 1;
        return 2;
    };
    if (text.empty()) {
        return {0, 0};
    }
    offset = std::min(offset, text.size());
    std::size_t at = offset;
    if (at > 0 && (at == text.size() || text[at] == '\n' ||
                   (kind(text[at]) != 1 && kind(text[at - 1]) == 1))) {
        --at;  // the end of a word, a line or the text: what comes before
    }
    const int k = kind(text[at]);
    std::size_t begin = at;
    std::size_t end = at + 1;
    while (begin > 0 && kind(text[begin - 1]) == k) --begin;
    while (end < text.size() && kind(text[end]) == k) ++end;
    // A double click on a line break selects nothing.
    if (text[at] == '\n') {
        return {offset, offset};
    }
    return {begin, end};
}
