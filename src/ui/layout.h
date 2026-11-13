#pragma once

class Editor;
struct Font;

// Per-frame order (see main.cpp):
//   HandleActiveFileInput -> UpdateLayoutScroll -> Layout -> ScrollEditorCursorIntoView
//   -> render -> DrawActiveFileOverlay -> ProcessLayoutActions

void Layout(Editor& editor);

bool IsLayoutModalOpen();

// Call before layout. Passes this frame's keyboard/mouse input to the active file.
// Returns true if its content changed or its cursor/selection moved.
bool HandleActiveFileInput(Editor& editor, Font* fonts);

// Call after input, before layout, so typing can interrupt an ongoing wheel gesture.
void UpdateLayoutScroll(float wheelX, float wheelY, float deltaTime, bool textChanged);

// Call after layout. Returns true if another layout pass is needed before drawing.
bool ScrollEditorCursorIntoView(Editor& editor, Font* fonts);

// Call after Clay has rendered. Lets the active file draw its cursor, selection, etc.
void DrawActiveFileOverlay(Editor& editor, Font* fonts);

// Call after EndDrawing(), while layout text references are no longer in use.
void ProcessLayoutActions(Editor& editor);
