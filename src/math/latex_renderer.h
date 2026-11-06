#pragma once

#include "raylib.h"
#include <string>

struct MathTexture {
    Texture2D texture = {0};

    float width = 0;
    float height = 0;
    float baseline = 0;

    bool IsValid() const {
        return texture.id != 0;
    }
};

class LatexRenderer {
public:
    //decided on SVG cause we wanted scaleability 
    //so the pipeline is svg
    MathTexture RenderSVG(const std::string& path);

    void Draw(
        const MathTexture& math,
        float x,
        float y
    );

    void Unload(MathTexture& math);
};
