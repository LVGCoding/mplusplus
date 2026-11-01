#pragma once

class Editor;

void Layout(Editor& editor);

// Call after EndDrawing(), while layout text references are no longer in use.
void ProcessLayoutActions(Editor& editor);
