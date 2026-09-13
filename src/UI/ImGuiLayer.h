#pragma once

#include "../Core/Defines.h"
#include "../Platform/Window.h"

// Thin wrapper around cimgui (Dear ImGui's C API) + its GLFW/OpenGL3
// backends. Keeps every ImGui/cimgui header out of the rest of the
// codebase -- callers only ever see this file.

void imgui_layer_init(window* wind);
void imgui_layer_shutdown(void);

// Call once per frame, after renderer_begin_frame() and before any ImGui
// ig*() calls (including app->on_frame, if it draws UI).
void imgui_layer_new_frame(void);

// Call once per frame, after app->on_frame and before renderer_end_frame()
// / the buffer swap -- this is what actually draws the UI on top of
// whatever the renderer already drew.
void imgui_layer_render(void);
