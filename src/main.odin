package main

import orui "../libs/orui/src"
import rl "vendor:raylib"

import "core:fmt"
import "core:os"

main :: proc() {
    // Sample taken from Raylib repo
    rl.SetConfigFlags({.WINDOW_RESIZABLE})

    rl.InitWindow(800, 600, "Test Window")
    for !rl.WindowShouldClose() {
        rl.BeginDrawing()
            rl.ClearBackground(rl.RAYWHITE)
            rl.DrawText("Congrats! You created your first window!", 190, 200, 20, rl.LIGHTGRAY)
        rl.EndDrawing()
      }
    rl.CloseWindow()

    ctx := new(orui.Context)
    orui.init(ctx)
    defer orui.destroy(ctx)


}