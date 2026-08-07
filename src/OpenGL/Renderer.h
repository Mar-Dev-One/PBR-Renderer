#pragma once

#include "../Core/Defines.h"

#include "../Platform/Window.h"

#include "glad/gl.h"

typedef struct renderer
{
    window* drawing_window;
}*renderer;

b8 init_renderer(window_descriptor init_window_desc);

renderer get_renderer();

void renderer_on_update();

void terminate_renderer();
