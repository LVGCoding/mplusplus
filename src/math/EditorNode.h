#pragma once

#include "clay.h"

#include <cstdint>
#include <random>
#include <string>
#include <utility>

class EditorNode {
public:
    EditorNode() : id_(GenerateId()) {}
    virtual ~EditorNode() = default;

    virtual void Render(float fontSize) = 0;
    virtual void KeyPress() = 0;
    virtual void MouseClick() = 0;

    // Serialize this node (and its children) back to M++/LaTeX source.
    virtual std::string ToText() const = 0;

    virtual std::pair<float, float> GetNWCorner() = 0;
    virtual std::pair<float, float> GetSize() = 0;
    virtual bool HasChild() = 0;

    // Random id assigned at construction; stable for the node's lifetime.
    std::uint32_t GetId() const { return id_; }

    // Clay element id for this node, consistent across frames so Clay can
    // track the element (hover, scroll, element data) between layouts.
    Clay_ElementId GetClayId() const { return CLAY_IDI("EditorNode", id_); }

private:
    static std::uint32_t GenerateId() {
        static std::mt19937 rng{std::random_device{}()};
        return rng();
    }

    std::uint32_t id_;
};
