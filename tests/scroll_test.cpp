// Wheel and trackpad scrolling (ui/WheelScroll.h).

#include "ui/WheelScroll.h"

#include <gtest/gtest.h>

namespace {

constexpr float LINE = 25;
constexpr float FRAME = 1.0f / 60;

// Total pixels scrolled from the given notches, one per frame at `gap`
// seconds apart, then frames until the scroll settles.
float ScrollNotches(WheelScroll& scroll, int count, double gap, float notch = -1) {
    float total = 0;
    double now = 0;
    int sent = 0;
    for (int frame = 0; frame < 2000; ++frame, now += FRAME) {
        float wheel = 0;
        if (sent < count && now >= sent * gap) {
            wheel = notch;
            ++sent;
        }
        total += scroll.Update(0, wheel, now, FRAME).y;
        if (sent == count && !scroll.Moving()) {
            break;
        }
    }
    return total;
}

}  // namespace

TEST(WheelScroll, ANotchScrollsThreeLinesSmoothly) {
    WheelScroll scroll(LINE);
    // The first frame covers only part of it.
    const float first = scroll.Update(0, -1, 0, FRAME).y;
    EXPECT_LT(first, 0);
    EXPECT_GT(first, -3 * LINE);
    float total = first;
    for (int i = 0; i < 60 && scroll.Moving(); ++i) {
        total += scroll.Update(0, 0, (i + 1) * FRAME, FRAME).y;
    }
    EXPECT_FALSE(scroll.Moving());
    EXPECT_NEAR(total, -3 * LINE, 0.5);
}

TEST(WheelScroll, QuickNotchesAccelerateAndSlowOnesDont) {
    WheelScroll slow(LINE);
    EXPECT_NEAR(ScrollNotches(slow, 6, 0.5), -6 * 3 * LINE, 1);
    WheelScroll fast(LINE);
    const float spun = ScrollNotches(fast, 6, 0.03);
    EXPECT_LT(spun, -6 * 3 * LINE * 1.5f);   // well over 1x
    EXPECT_GT(spun, -6 * 3 * LINE * 4.0f);   // capped
}

TEST(WheelScroll, ReversingCancelsTheRestAndStopDropsIt) {
    WheelScroll scroll(LINE);
    scroll.Update(0, -1, 0, FRAME);
    // Back up at once: the rest of the downward scroll is dropped.
    const float back = ScrollNotches(scroll, 1, 1, 1);
    EXPECT_GT(back, 0);
    scroll.Update(0, -1, 5, FRAME);
    scroll.Stop();
    EXPECT_FALSE(scroll.Moving());
    EXPECT_EQ(scroll.Update(0, 0, 5.1, FRAME).y, 0);
}

TEST(WheelScroll, TrackpadsFollowTheFingers) {
    WheelScroll scroll(LINE);
    float total = 0;
    for (int i = 0; i < 10; ++i) {
        total += scroll.Update(0, -0.4f, i * FRAME, FRAME).y;
    }
    for (int i = 10; i < 100 && scroll.Moving(); ++i) {
        total += scroll.Update(0, 0, i * FRAME, FRAME).y;
    }
    EXPECT_NEAR(total, -10 * 0.4f * WheelScroll::TRACKPAD_PIXELS, 1);
}
