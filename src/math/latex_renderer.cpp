#include <cstdio>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <iostream>


#include "LatexRenderer.h"
#include "SVGPreprocessor.h"
#include "defer.h"

#include "../lib/nanosvg/nanosvg.h"
#include "../lib/nanosvg/nanosvgrast.h"

MathTexture LatexRenderer::RenderSVG(const std::string& path) {
    MathTexture result;

    if (!FileExists(path.c_str())) {
        std::printf("SVG file not found: %s\n", path.c_str());
        return result;
    }

    std::string processedPath = "../latexGen/processedLatex.svg";

    SVGPreprocessor preprocessor;

    if (!preprocessor.Process(path, processedPath)) {
        return result;
    }


 

    // Parse the SVG
    NSVGimage* image = nsvgParseFromFile(processedPath.c_str(), "px", 96.0f);
    if (!image) {
        std::printf("Failed to parse SVG\n");
        return result;
    }
    defer (nsvgDelete(image));

    // std::printf("SVG loaded: %.2f x %.2f\n", image->width, image->height);


    // Create the rasterizer
    NSVGrasterizer* rasterizer = nsvgCreateRasterizer();
    if (!rasterizer) {
        nsvgDelete(image);
        return result;
    }
    defer(nsvgDeleteRasterizer(rasterizer));

    // Render at 2x resolution for better quality
    constexpr float scale = 2.0f;
    int width = static_cast<int>(std::ceil(image->width * scale));
    int height = static_cast<int>(std::ceil(image->height * scale));

    // Allocate RGBA pixel memory
    unsigned char* pixels = static_cast<unsigned char*>(MemAlloc(width * height * 4));
    defer(MemFree(pixels));
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


    return result;
}

MathTexture LatexRenderer::RenderLatex(const std::string& latex){
    std::cout << "Working directory: " << std::filesystem::current_path() << '\n';
    std::ofstream file("../latexGen/newLatex.tex");
    if (!file.is_open()) {
        std::printf("\nFILE NOT OPEN\n");
    }
    file << "\\documentclass{article}";
    file << "\\usepackage{amsmath}";
    file << "\\pagestyle{empty}";
    file << "\\begin{document}";
    file << "\\[";
    file << latex;
    file << "\\]";
    file << "\\end{document}";
    file.close();

    std::string createSVG = "./../latexGen/stringToSVG.sh";
    int result = std::system(createSVG.c_str());
    
    if (result != 0) {
        std::printf("\nBASH PROKEN\n");
    }

    
    return RenderSVG("../latexGen/newSVG.svg");

}

void LatexRenderer::Draw(const MathTexture& math, float x, float y) {
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
