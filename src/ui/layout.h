#pragma once

#include "editor/file.h"
#include "raylib.h"

class Editor;

void Layout(Editor& editor);

bool IsLayoutModalOpen();

// Call after EndDrawing(), while layout text references are no longer in use.
void ProcessLayoutActions(Editor& editor);
void DrawEditorCursor(const File& file, Font font);
void DrawEditorSelection(const File& file, Font font);