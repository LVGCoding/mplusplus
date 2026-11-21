#pragma once

// Turns mouse wheel and trackpad input into smooth, accelerated scrolling.
//
// Clay on its own moves 10 pixels per wheel notch and jumps there at once.
// Instead, each notch asks for a few lines, a quick run of notches in one
// direction asks for more per notch (up to MAX_ACCELERATION times), and the
// distance asked for is covered over a few frames rather than in one jump.
// Trackpads report fractions of a notch; those follow the fingers without
// extra acceleration (the system already accelerates them).
//
// No raylib or Clay calls, so it can be tested; the caller passes the time.

#include <algorithm>
#include <cmath>

class WheelScroll {
public:
    static constexpr float LINES_PER_NOTCH = 3.0f;
    // Pixels per trackpad unit (a fraction of a notch).
    static constexpr float TRACKPAD_PIXELS = 25.0f;
    // Notches closer together than this (seconds) build up speed.
    static constexpr double FAST_NOTCH_GAP = 0.12;
    static constexpr float ACCELERATION_STEP = 0.35f;
    static constexpr float MAX_ACCELERATION = 4.0f;
    // How fast the remaining distance is covered: about 95% in 0.15 s.
    static constexpr float SMOOTHING_RATE = 20.0f;

    explicit WheelScroll(float lineHeight) : lineHeight_(lineHeight) {}

    struct Step {
        float x = 0;
        float y = 0;
    };

    // The pixels to scroll this frame, given this frame's wheel movement
    // (notches; positive is up/left, as raylib reports it), the time now
    // and the frame's length, both in seconds.
    Step Update(float wheelX, float wheelY, double now, float deltaTime) {
        AddAxis(wheelX, now, pendingX_, lastX_);
        AddAxis(wheelY, now, pendingY_, lastY_);
        const float share = 1.0f - std::exp(-SMOOTHING_RATE * std::max(deltaTime, 0.0f));
        Step step{pendingX_ * share, pendingY_ * share};
        pendingX_ -= step.x;
        pendingY_ -= step.y;
        // Finish once less than half a pixel is left, so the scroll lands
        // exactly where the notches asked for.
        if (std::fabs(pendingX_) < 0.5f) {
            step.x += pendingX_;
            pendingX_ = 0;
        }
        if (std::fabs(pendingY_) < 0.5f) {
            step.y += pendingY_;
            pendingY_ = 0;
        }
        return step;
    }

    // Drop any scrolling still to come (the user typed, or a popup opened).
    void Stop() {
        pendingX_ = pendingY_ = 0;
        lastX_ = lastY_ = {};
    }

    bool Moving() const {
        return pendingX_ != 0 || pendingY_ != 0;
    }

private:
    struct Last {
        double time = -1;
        float direction = 0;
        int streak = 0;
    };

    float lineHeight_;
    float pendingX_ = 0;
    float pendingY_ = 0;
    Last lastX_;
    Last lastY_;

    void AddAxis(float wheel, double now, float& pending, Last& last) {
        if (wheel == 0) {
            return;
        }
        const float direction = wheel > 0 ? 1.0f : -1.0f;
        const bool notches = std::fabs(wheel - std::round(wheel)) < 1e-3f;
        if (!notches) {
            last = {};
            pending += wheel * TRACKPAD_PIXELS;
            return;
        }
        const bool quick = last.time >= 0 && now - last.time < FAST_NOTCH_GAP &&
                           last.direction == direction;
        last.streak = quick ? last.streak + 1 : 0;
        last.time = now;
        last.direction = direction;
        // Scrolling back the other way cancels what was still to come.
        if (pending * direction < 0) {
            pending = 0;
        }
        const float acceleration =
            std::min(1.0f + ACCELERATION_STEP * static_cast<float>(last.streak), MAX_ACCELERATION);
        pending += wheel * LINES_PER_NOTCH * lineHeight_ * acceleration;
    }
};
