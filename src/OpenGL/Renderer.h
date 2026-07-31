#pragma once

#include "../Core/Defines.h"

#include "../Platform/Window.h"

typedef struct renderer *renderer;

b8 init_renderer(window_descriptor* windows_desc, uint8 windows_number);

void renderer_on_update();

void terminate_renderer();
