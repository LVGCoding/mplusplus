#pragma once

class Editor;
struct File;
struct Font;

void Layout(Editor& editor);

bool IsLayoutModalOpen();

// Call after input, before layout, so typing can interrupt an ongoing wheel gesture.
void UpdateLayoutScroll(float wheelX, float wheelY, float deltaTime, bool textChanged);

// Call after layout. Returns true if another layout pass is needed before drawing.
bool ScrollEditorCursorIntoView(const File& file, Font* fonts);

// Call after EndDrawing(), while layout text references are no longer in use.
void ProcessLayoutActions(Editor& editor);
