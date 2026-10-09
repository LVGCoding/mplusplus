package main

import orui "../libs/orui/src"
import rl "vendor:raylib"

import "core:fmt"
import "core:os"

main :: proc() {
    // Sample taken from Raylib repo
    rl.SetConfigFlags({.WINDOW_RESIZABLE})

    ctx := new(orui.Context)
    orui.init(ctx)
    defer orui.destroy(ctx)

    rl.InitWindow(800, 600, "Test Window")
    for !rl.WindowShouldClose() {
        rl.BeginDrawing()
        orui.begin(ctx, 800, 600)
        
        rl.ClearBackground(rl.LIME)
        
        rl.EndDrawing()
      }
    rl.CloseWindow()
}