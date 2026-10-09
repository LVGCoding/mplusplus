package main

import orui "../libs/orui/src"
import rl "vendor:raylib"

import "core:fmt"
import "core:os"

main :: proc() {
    // Sample taken from Raylib repo
    rl.SetConfigFlags({.WINDOW_RESIZABLE})

    ctx := new(orui.Context)
    defer free(ctx)

    orui.init(ctx)
    defer orui.destroy(ctx)

    ctx.default_font = rl.GetFontDefault()
    

    rl.InitWindow(800, 600, "Test Window")
    for !rl.WindowShouldClose() {
        rl.BeginDrawing()
        
        width := rl.GetScreenWidth()
        height := rl.GetScreenHeight()
        orui.begin(ctx, width, height)
        
        rl.ClearBackground(rl.LIME)
        
        rl.EndDrawing()
      }
    rl.CloseWindow()
}