#pragma once

#include "Core/Defines.h"
#include "glad/gl.h"
#include "Window.h"


typedef struct renderer
{
    window* current_window;

} renderer;

b8 renderer_init(renderer* renderer);

b8 set_target_window(renderer* renderer, window* window);

b8 renderer_on_update(renderer* renderer, f32 delta_time);

void renderer_destroy(renderer* renderer);
