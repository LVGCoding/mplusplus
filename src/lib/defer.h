#pragma once

// Run cleanup at scope exit, including early returns. Callbacks capture by reference,
// so referenced resources must outlive the guard. Guards run in reverse declaration order.
template <typename Callback>
class ScopeExit {
public:
    explicit ScopeExit(Callback callback) : callback_(callback) {}

    ~ScopeExit() {
        callback_();
    }

private:
    Callback callback_;
};

template <typename Callback>
ScopeExit<Callback> MakeScopeExit(Callback callback) {
    return ScopeExit<Callback>(callback);
}

// Expand the counter before token pasting so each cleanup guard has a unique local name.
#define DEFER_JOIN_DIRECT(left, right) left##right
#define DEFER_JOIN(left, right) DEFER_JOIN_DIRECT(left, right)
#define DEFER(code) auto DEFER_JOIN(deferredCleanup, __COUNTER__) = MakeScopeExit([&]() { code; })
