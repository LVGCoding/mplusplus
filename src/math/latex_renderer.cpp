#include <cstdio>
#include "latex_renderer.h"

#include "../lib/nanosvg/nanosvg.h"
#include "../lib/nanosvg/nanosvgrast.h"

#include <cmath>

MathTexture LatexRenderer::RenderSVG(const std::string& path) {
    MathTexture result;
    if (!FileExists(path.c_str())) {
        std::printf("SVG file not found: %s\n", path.c_str());
        return result;
    }
    // Parse the SVG
    NSVGimage* image = nsvgParseFromFile(path.c_str(), "px", 96.0f);

    if (!image) {
        std::printf("Failed to parse SVG\n");
        return result;
    }

    std::printf("SVG loaded: %.2f x %.2f\n", image->width, image->height);

    if (!image) {
        return result;
    }

    // Create the rasterizer
    NSVGrasterizer* rasterizer = nsvgCreateRasterizer();

    if (!rasterizer) {
        nsvgDelete(image);
        return result;
    }

    // Render at 2x resolution for better quality
    constexpr float scale = 2.0f;

    int width = static_cast<int>(std::ceil(image->width * scale));

    int height = static_cast<int>(std::ceil(image->height * scale));

    // Allocate RGBA pixel memory
    unsigned char* pixels = static_cast<unsigned char*>(MemAlloc(width * height * 4));

    // Rasterize SVG
    nsvgRasterize(
        rasterizer,
        image,
        0, 0,
        scale,
        pixels,
        width,
        height,
        width * 4
    );

    // Convert to a Raylib image
    Image rayImage = {
        pixels,
        width,
        height,
        1,
        PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
    };

    // Create GPU texture
    result.texture = LoadTextureFromImage(rayImage);

    result.width = static_cast<float>(width) / scale;
    result.height = static_cast<float>(height) / scale;

    // Clean up CPU-side resources
    MemFree(pixels);

    nsvgDeleteRasterizer(rasterizer);
    nsvgDelete(image);

    return result;
}

void LatexRenderer::Draw(
    const MathTexture& math,
    float x,
    float y
) {
    if (!math.IsValid()) {
        return;
    }

    DrawTexture(math.texture, static_cast<int>(x), static_cast<int>(y), WHITE);
}

void LatexRenderer::Unload(MathTexture& math) {
    if (math.IsValid()) {
        UnloadTexture(math.texture);
        math.texture = {0};
    }
}
