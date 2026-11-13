#pragma once

// Shared includes for layout.cpp and its internal layout_*.h helpers.
// Kept in the same order as the original layout.cpp.

#include "layout.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_map>

#include "editor/editor.h"
#include "clay.h"
#include "raylib.h"
#include "file_dialog.h"
#include "font.h"
#include "clay_renderer_raylib.h"
#include "EditorView.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
